/* main.c -- MQTT client example
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
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"

#include "mqtt/mqtt.h"
#include "mqtt/wifi.h"
#include "mqtt/config.h"
#include "mqtt/debug.h"

#include "driver/caliper.h"
#include "driver/dial.h"
#include "driver/max6675.h"

MQTT_Client mqttClient;

static volatile os_timer_t sensor_timer;
static volatile bool broker_established = false;
static char clientid[66];
#define SENDBUFFERSIZE 128
static char sendbuffer[SENDBUFFERSIZE];

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

void wifiConnectCb(uint8_t status)
{
  char hwaddr[6];
  wifi_get_macaddr(0, hwaddr);
  os_sprintf(clientid, "/" MACSTR , MAC2STR(hwaddr));

	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	}
}

void mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected! will use %s as MQTT channel \r\n", clientid);
	broker_established = true;
	MQTT_Subscribe(&mqttClient, "/test/topic");
	MQTT_Subscribe(&mqttClient, "/test2/topic");
}

void mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
	broker_established = false;
}

void mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
	char topicBuf[64], dataBuf[64];
	MQTT_Client* client = (MQTT_Client*)args;

	os_memcpy(topicBuf, topic, topic_len);
	topicBuf[topic_len] = 0;

	os_memcpy(dataBuf, data, data_len);
	dataBuf[data_len] = 0;

	INFO("MQTT topic: %s, data: %s \r\n", topicBuf, dataBuf);

	/* Echo back to /echo channel*/
	MQTT_Publish(client, "/echo", dataBuf, data_len, 0, 0);
}

void
readSampleAndPublish(void *arg)
{
  int nextPeriod = CALIPER_SAMPLE_PERIOD;
  if (broker_established) {
    int bytesWritten = 0;
    if (max6675_readTempAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten, true)) {
    //if (readCaliperAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten)) {
    //if (readDialAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten)) {
      INFO("MQTT readSampleAndPublish: received %s\r\n", sendbuffer);
      MQTT_Publish( &mqttClient, clientid, sendbuffer, bytesWritten, 0, false);
    } else {
      // do a quick resample
      nextPeriod = 250;
    }
  }
  os_timer_disarm(&sensor_timer);
  os_timer_arm(&sensor_timer, nextPeriod, 0);
}

void user_init(void)
{
  // Make os_printf working again. Baud:115200,n,8,1
  stdoutInit();

	os_delay_us(1000000);

	CFG_Load();
	gpio_init();
	//caliperInit();
	//dialInit();
	max6675_init();

	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, SEC_NONSSL);
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	INFO("\r\nConnecting to SSID:");INFO(sysCfg.sta_ssid);INFO("\r\n");
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	INFO("\r\nSystem started ...\r\n");

	os_timer_disarm(&sensor_timer);

  //Setup timer
  os_timer_setfn(&sensor_timer, (os_timer_func_t *)readSampleAndPublish, NULL);
  os_timer_arm(&sensor_timer, CALIPER_SAMPLE_PERIOD, 0);
}
