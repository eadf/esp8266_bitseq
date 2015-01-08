/* mqtt.c
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt/mqtt_msg.h"
#include "mqtt/debug.h"
#include "user_config.h"
#include "mqtt/config.h"
#include "mqtt/str_queue.h"
#include "mqtt/mqtt.h"


#define MQTT_TASK_PRIO        		0
#define MQTT_TASK_QUEUE_SIZE    	1

#define MAX_TOPIC_QUEUE		 		5
#define MAX_TOPIC_LEN		 		64

#define MAX_PUBLISH_QUEUE		 	5
#define MAX_PUBLISH_LEN		 		1024



unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;

os_event_t mqtt_procTaskQueue[MQTT_TASK_QUEUE_SIZE];


LOCAL void ICACHE_FLASH_ATTR
mqtt_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
	struct espconn *pConn = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pConn->reverse;


	if(ipaddr == NULL)
	{
		INFO("DNS: Found, but got no ip, try to reconnect\r\n");
		client->connState = TCP_RECONNECT_REQ;
		return;
	}

	INFO("DNS: found ip %d.%d.%d.%d\n",
			*((uint8 *) &ipaddr->addr),
			*((uint8 *) &ipaddr->addr + 1),
			*((uint8 *) &ipaddr->addr + 2),
			*((uint8 *) &ipaddr->addr + 3));

	if(client->ip.addr == 0 && ipaddr->addr != 0)
	{
		os_memcpy(client->pCon->proto.tcp->remote_ip, &ipaddr->addr, 4);
		if(client->security){
			espconn_secure_connect(client->pCon);
		}
		else {
			espconn_connect(client->pCon);
		}

		client->connState = TCP_CONNECTING;
		INFO("TCP: connecting...\r\n");
	}

	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}



LOCAL void ICACHE_FLASH_ATTR
deliver_publish(MQTT_Client* client, uint8_t* message, int length)
{

	mqtt_event_data_t event_data;

	event_data.topic_length = length;
	event_data.topic = mqtt_get_publish_topic(message, &event_data.topic_length);

	event_data.data_length = length;
	event_data.data = mqtt_get_publish_data(message, &event_data.data_length);

	if(client->dataCb)
		client->dataCb((uint32_t*)client, event_data.topic, event_data.topic_length, event_data.data, event_data.data_length);

}

LOCAL void ICACHE_FLASH_ATTR
deliver_publish_continuation(MQTT_Client* client, uint16_t offset, uint8_t* buffer, uint16_t length)
{
  mqtt_event_data_t event_data;

  event_data.type = MQTT_EVENT_TYPE_PUBLISH_CONTINUATION;
  event_data.topic_length = 0;
  event_data.topic = NULL;
  event_data.data_length = length;
  event_data.data_offset = offset;
  event_data.data = (char*)buffer;
  ((char*)event_data.data)[event_data.data_length] = '\0';

 //process_post_synch(state->calling_process, mqtt_event, &event_data);
}

/**
  * @brief  Client received callback function.
  * @param  arg: contain the ip link information
  * @param  pdata: received data
  * @param  len: the lenght of received data
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_recv(void *arg, char *pdata, unsigned short len)
{
	uint8_t msg_type;
	uint8_t msg_qos;

	uint16_t msg_id;

	struct espconn *pCon = (struct espconn*)arg;
	MQTT_Client *client = (MQTT_Client *)pCon->reverse;

	INFO("TCP: data received\r\n");
	if(len < MQTT_BUF_SIZE && len > 0){
		memcpy(client->mqtt_state.in_buffer, pdata, len);

		switch(client->connState){
		case MQTT_CONNECT_SENDING:
			if(mqtt_get_type(client->mqtt_state.in_buffer) != MQTT_MSG_TYPE_CONNACK){
				INFO("MQTT: Invalid packet\r\n");
				if(client->security){
					espconn_secure_disconnect(client->pCon);
				}
				else {
					espconn_disconnect(client->pCon);
				}

			} else {
				INFO("MQTT: Connected to %s:%d\r\n", client->host, client->port);
				client->connState = MQTT_DATA;
				if(client->connectedCb)
					client->connectedCb((uint32_t*)client);
			}
			break;
		case MQTT_SUBSCIBE_SENDING:
			if(mqtt_get_type(client->mqtt_state.in_buffer) != MQTT_MSG_TYPE_SUBACK){
				INFO("MQTT: Invalid packet\r\n");
				if(client->security){
					espconn_secure_disconnect(client->pCon);
				}
				else{
					espconn_disconnect(client->pCon);
				}
			} else {
				if(QUEUE_IsEmpty(&client->topicQueue)){
					client->connState = MQTT_DATA;

				} else {
					client->connState = MQTT_SUBSCIBE_SEND;
				}
				INFO("MQTT: Subscribe successful\r\n");

			}
			break;
		case MQTT_DATA:
			client->mqtt_state.message_length_read = len;
			client->mqtt_state.message_length = mqtt_get_total_length(client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);
			msg_type = mqtt_get_type(client->mqtt_state.in_buffer);
			msg_qos = mqtt_get_qos(client->mqtt_state.in_buffer);
			msg_id = mqtt_get_id(client->mqtt_state.in_buffer, client->mqtt_state.in_buffer_length);
			switch(msg_type)
			{
			  case MQTT_MSG_TYPE_SUBACK:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_SUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
				  INFO("MQTT: Subscribe successful to %s:%d\r\n", client->host, client->port);
				break;
			  case MQTT_MSG_TYPE_UNSUBACK:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_UNSUBSCRIBE && client->mqtt_state.pending_msg_id == msg_id)
				  INFO("MQTT: UnSubscribe successful\r\n");
				break;
			  case MQTT_MSG_TYPE_PUBLISH:
				if(msg_qos == 1)
					client->mqtt_state.outbound_message = mqtt_msg_puback(&client->mqtt_state.mqtt_connection, msg_id);
				else if(msg_qos == 2)
					client->mqtt_state.outbound_message = mqtt_msg_pubrec(&client->mqtt_state.mqtt_connection, msg_id);

				deliver_publish(client, client->mqtt_state.in_buffer, client->mqtt_state.message_length_read);
				break;
			  case MQTT_MSG_TYPE_PUBACK:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id){
				  INFO("MQTT: Publish successful\r\n");
				  if(client->publishedCb)
					  client->publishedCb((uint32_t*)client);
				}

				break;
			  case MQTT_MSG_TYPE_PUBREC:
				  client->mqtt_state.outbound_message = mqtt_msg_pubrel(&client->mqtt_state.mqtt_connection, msg_id);
				break;
			  case MQTT_MSG_TYPE_PUBREL:
				  client->mqtt_state.outbound_message = mqtt_msg_pubcomp(&client->mqtt_state.mqtt_connection, msg_id);
				break;
			  case MQTT_MSG_TYPE_PUBCOMP:
				if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == msg_id){
				  INFO("MQTT: Public successful\r\n");
				  if(client->publishedCb)
					  client->publishedCb((uint32_t*)client);
				}
				break;
			  case MQTT_MSG_TYPE_PINGREQ:
				  client->mqtt_state.outbound_message = mqtt_msg_pingresp(&client->mqtt_state.mqtt_connection);
				break;
			  case MQTT_MSG_TYPE_PINGRESP:
				// Ignore
				break;
			}
			// NOTE: this is done down here and not in the switch case above
			// because the PSOCK_READBUF_LEN() won't work inside a switch
			// statement due to the way protothreads resume.
			if(msg_type == MQTT_MSG_TYPE_PUBLISH)
			{
			  uint16_t len;

			  // adjust message_length and message_length_read so that
			  // they only account for the publish data and not the rest of the
			  // message, this is done so that the offset passed with the
			  // continuation event is the offset within the publish data and
			  // not the offset within the message as a whole.
			  len = client->mqtt_state.message_length_read;
			  mqtt_get_publish_data(client->mqtt_state.in_buffer, &len);
			  len = client->mqtt_state.message_length_read - len;
			  client->mqtt_state.message_length -= len;
			  client->mqtt_state.message_length_read -= len;

			  if(client->mqtt_state.message_length_read < client->mqtt_state.message_length)
			  {
				  msg_type = MQTT_PUBLISH_RECV;
			  }

			}
			break;
			case MQTT_PUBLISH_RECV:
				/*
				 * Long publish message, not implement yet
				 * TODO: Implement method used deliver_publish_continuation
				 */

				break;
		}
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
  * @brief  Client send over callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_sent_cb(void *arg)
{
	struct espconn *pCon = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pCon->reverse;
	INFO("TCP: Sent\r\n");
	if(client->connState == MQTT_DATA){
		if(client->publishedCb)
			client->publishedCb((uint32_t*)client);
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR mqtt_timer(void *arg)
{
	MQTT_Client* client = (MQTT_Client*)arg;
	if(client->connState == MQTT_DATA){
		client->keepAliveTick ++;
		if(client->keepAliveTick > client->mqtt_state.connect_info->keepalive){

			INFO("\r\nMQTT: Send keepalive packet to %s:%d!\r\n", client->host, client->port);
			client->mqtt_state.outbound_message = mqtt_msg_pingreq(&client->mqtt_state.mqtt_connection);
			client->keepAliveTick = 0;
			system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
		}

	} else if(client->connState == TCP_RECONNECT_REQ){
		client->reconnectTick ++;
		if(client->reconnectTick > MQTT_RECONNECT_TIMEOUT) {
			client->reconnectTick = 0;
			client->connState = TCP_RECONNECT;
			system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
		}
	}
}

void ICACHE_FLASH_ATTR
mqtt_tcpclient_discon_cb(void *arg)
{

	struct espconn *pespconn = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pespconn->reverse;
	INFO("TCP: Disconnected callback\r\n");
	client->connState = TCP_RECONNECT_REQ;
	if(client->disconnectedCb)
		client->disconnectedCb((uint32_t*)client);

	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}



/**
  * @brief  Tcp client connect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_connect_cb(void *arg)
{
	struct espconn *pCon = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pCon->reverse;

	espconn_regist_disconcb(client->pCon, mqtt_tcpclient_discon_cb);
	espconn_regist_recvcb(client->pCon, mqtt_tcpclient_recv);////////
	espconn_regist_sentcb(client->pCon, mqtt_tcpclient_sent_cb);///////
	INFO("MQTT: Connected to broker %s:%d\r\n", client->host, client->port);
	client->connState = MQTT_CONNECT_SEND;
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

/**
  * @brief  Tcp client connect repeat callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
mqtt_tcpclient_recon_cb(void *arg, sint8 errType)
{
	struct espconn *pCon = (struct espconn *)arg;
	MQTT_Client* client = (MQTT_Client *)pCon->reverse;

	INFO("TCP: Reconnect to %s:%d\r\n", client->host, client->port);

	client->connState = TCP_RECONNECT_REQ;

	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);

}


void ICACHE_FLASH_ATTR
MQTT_Publish(MQTT_Client *client, const char* topic, const char* data, int data_length, int qos, int retain)
{
	if(client->connState != MQTT_DATA)
		return;
	INFO("MQTT: sending publish...\r\n");
	client->mqtt_state.outbound_message = mqtt_msg_publish(&client->mqtt_state.mqtt_connection,
                                                 topic, data, data_length,
                                                 qos, retain,
                                                 &client->mqtt_state.pending_msg_id);
  system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);

}

void ICACHE_FLASH_ATTR
MQTT_Subscribe(MQTT_Client *client, char* topic)
{
	INFO("MQTT: subscribe, topic\"%s\" at broker %s:%d\r\n", topic, client->host, client->port);

	if(QUEUE_Puts(&client->topicQueue, topic) == -1){
		INFO("MQTT: Exceed the amount of queues\r\n");
	} else {
		if(client->connState == MQTT_DATA)
			client->connState = MQTT_SUBSCIBE_SEND;
	}
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)client);
}

void ICACHE_FLASH_ATTR
MQTT_Task(os_event_t *e)
{
	MQTT_Client* client = (MQTT_Client*)e->par;
	uint8_t topic[64];

	switch(client->connState){

	case TCP_RECONNECT_REQ:
		break;
	case TCP_RECONNECT:
		MQTT_Connect(client);
		INFO("TCP:Reconect to: %s:%d\r\n", client->host, client->port);
		client->connState = TCP_CONNECTING;
		break;
	case MQTT_CONNECT_SEND:

		mqtt_msg_init(&client->mqtt_state.mqtt_connection, client->mqtt_state.out_buffer, client->mqtt_state.out_buffer_length);
		client->mqtt_state.outbound_message = mqtt_msg_connect(&client->mqtt_state.mqtt_connection, client->mqtt_state.connect_info);
		if(client->security){
			espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
		}
		else
		{
			espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
		}

		INFO("MQTT: Send mqtt connection info, to broker %s:%d\r\n", client->host, client->port);
		client->connState = MQTT_CONNECT_SENDING;
		client->mqtt_state.outbound_message = NULL;
		break;
	case MQTT_SUBSCIBE_SEND:
		if(QUEUE_Gets(&client->topicQueue, topic) == 0){
			client->mqtt_state.outbound_message = mqtt_msg_subscribe(&client->mqtt_state.mqtt_connection,
																	topic, 0,
																   &client->mqtt_state.pending_msg_id);
			client->mqtt_state.pending_msg_type = MQTT_MSG_TYPE_SUBSCRIBE;
			if(client->security){
				espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
			}
			else{
				espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
			}

			client->mqtt_state.outbound_message = NULL;
			client->connState = MQTT_SUBSCIBE_SENDING;
		} else {
			client->connState = MQTT_DATA;
		}

		break;
	case MQTT_DATA:
		if(client->mqtt_state.outbound_message != NULL){
			if(client->security){
				espconn_secure_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
			}
			else{
				espconn_sent(client->pCon, client->mqtt_state.outbound_message->data, client->mqtt_state.outbound_message->length);
			}

			client->mqtt_state.outbound_message = NULL;
			if(client->mqtt_state.pending_msg_type == MQTT_MSG_TYPE_PUBLISH && client->mqtt_state.pending_msg_id == 0)
				INFO("MQTT: Publish message is done!\r\n");
			break;
		}
		break;
	}
}


void MQTT_InitConnection(MQTT_Client *mqttClient, uint8_t* host, uint32 port, uint8_t security)
{
	INFO("MQTT_InitConnection\r\n");
	os_memset(mqttClient, 0, sizeof(MQTT_Client));

	mqttClient->pCon = (struct espconn *)os_zalloc(sizeof(struct espconn));
	mqttClient->pCon->type = ESPCONN_TCP;
	mqttClient->pCon->state = ESPCONN_NONE;
	mqttClient->pCon->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	mqttClient->pCon->proto.tcp->local_port = espconn_port();
	mqttClient->pCon->proto.tcp->remote_port = port;
	mqttClient->host = host;
	mqttClient->port = port;
	mqttClient->security = security;
	mqttClient->pCon->reverse = mqttClient;


	espconn_regist_connectcb(mqttClient->pCon, mqtt_tcpclient_connect_cb);
	espconn_regist_reconcb(mqttClient->pCon, mqtt_tcpclient_recon_cb);

}



void MQTT_InitClient(MQTT_Client *mqttClient, uint8_t* client_id, uint8_t* client_user, uint8_t* client_pass, uint32_t keepAliveTime)
{
	INFO("MQTT_InitClient\r\n");

	os_memset(&mqttClient->connect_info, 0, sizeof(mqtt_connect_info_t));
	mqttClient->connect_info.client_id = client_id;
	mqttClient->connect_info.username = client_user;
	mqttClient->connect_info.password = client_pass;

	mqttClient->connect_info.keepalive = keepAliveTime;
	mqttClient->connect_info.clean_session = 1;
	//mqttClient->mqtt_state = (mqtt_state_t *)os_zalloc(sizeof(mqtt_state_t));



	mqttClient->mqtt_state.in_buffer = (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
	mqttClient->mqtt_state.in_buffer_length = MQTT_BUF_SIZE;
	mqttClient->mqtt_state.out_buffer =  (uint8_t *)os_zalloc(MQTT_BUF_SIZE);
	mqttClient->mqtt_state.out_buffer_length = MQTT_BUF_SIZE;
	mqttClient->mqtt_state.connect_info = &mqttClient->connect_info;
	mqttClient->keepAliveTick = 0;
	mqttClient->reconnectTick = 0;

	os_timer_disarm(&mqttClient->mqttTimer);
	os_timer_setfn(&mqttClient->mqttTimer, (os_timer_func_t *)mqtt_timer, mqttClient);
	os_timer_arm(&mqttClient->mqttTimer, 1000, 1);

	QUEUE_Init(&mqttClient->topicQueue, MAX_TOPIC_LEN, MAX_TOPIC_QUEUE);

	system_os_task(MQTT_Task, MQTT_TASK_PRIO, mqtt_procTaskQueue, MQTT_TASK_QUEUE_SIZE);
	system_os_post(MQTT_TASK_PRIO, 0, (os_param_t)mqttClient);

}

void MQTT_Connect(MQTT_Client *mqttClient)
{
	if(UTILS_StrToIP(mqttClient->host, &mqttClient->pCon->proto.tcp->remote_ip)) {
		INFO("TCP: Connect to ip  %s:%d\r\n", mqttClient->host, mqttClient->port);
		if(mqttClient->security){
			espconn_secure_connect(mqttClient->pCon);
		}
		else {
			espconn_connect(mqttClient->pCon);
		}

	}
	else {
		INFO("TCP: Connect to domain %s:%d\r\n", mqttClient->host, mqttClient->port);
		espconn_gethostbyname(mqttClient->pCon, mqttClient->host, &mqttClient->ip, mqtt_dns_found);
	}


	mqttClient->connState = TCP_CONNECTING;
}
void MQTT_OnConnected(MQTT_Client *mqttClient, MqttCallback connectedCb)
{
	mqttClient->connectedCb = connectedCb;
}
void MQTT_OnDisconnected(MQTT_Client *mqttClient, MqttCallback disconnectedCb)
{
	mqttClient->disconnectedCb = disconnectedCb;
}
void MQTT_OnData(MQTT_Client *mqttClient, MqttDataCallback dataCb)
{
	mqttClient->dataCb = dataCb;
}
void MQTT_OnPublished(MQTT_Client *mqttClient, MqttCallback publishedCb)
{
	mqttClient->publishedCb = publishedCb;
}
