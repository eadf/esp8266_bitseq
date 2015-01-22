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

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#ifdef USE_DIAL_SENSOR
#undef USE_CALIPER_SENSOR
#undef USE_MAX75_SENSOR
#undef USE_WATT_SENSOR
#endif

#ifdef USE_CALIPER_SENSOR
#undef USE_DIAL_SENSOR
#undef USE_MAX75_SENSOR
#undef USE_WATT_SENSOR
#endif

#ifdef USE_MAX75_SENSOR
#undef USE_DIAL_SENSOR
#undef USE_CALIPER_SENSOR
#undef USE_WATT_SENSOR
#endif

#ifdef USE_WATT_SENSOR
#undef USE_DIAL_SENSOR
#undef USE_CALIPER_SENSOR
#undef USE_MAX75_SENSOR
#endif

MQTT_Client mqttClient;

static volatile os_timer_t sensor_timer;
static volatile bool broker_established = false;
static char clientid[66];
#define SENDBUFFERSIZE 128
static char sendbuffer[SENDBUFFERSIZE];

void wifiConnectCb(uint8_t status);
void mqttConnectedCb(uint32_t *args);
void mqttConnectedCb(uint32_t *args);
void mqttPublishedCb(uint32_t *args);
void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len);
void mqttDisconnectedCb(uint32_t *args);

void performMaxSensorSamplingTimer(void);
void initiateCaliperSensorSamplingTimer(void);
void initiateDialSensorSamplingTimer(void);
void dialSensorDataCb(void);
void initiateWattSensorSamplingTimer(void);
void wattSensorDataCb(void);
void caliperSensorDataCb(void);
void user_init(void);

void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status)
{
  char hwaddr[6];
  wifi_get_macaddr(0, hwaddr);
  os_sprintf(clientid, "/" MACSTR , MAC2STR(hwaddr));
  
	if(status == STATION_GOT_IP){
		MQTT_Connect(&mqttClient);
	}
}

void ICACHE_FLASH_ATTR
mqttConnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Connected! will use %s as MQTT topic \r\n", clientid);
	broker_established = true;
	MQTT_Subscribe(&mqttClient, "/test/topic");
	MQTT_Subscribe(&mqttClient, "/test2/topic");
}

void ICACHE_FLASH_ATTR
mqttDisconnectedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Disconnected\r\n");
	broker_established = false;
}

void ICACHE_FLASH_ATTR
mqttPublishedCb(uint32_t *args)
{
	MQTT_Client* client = (MQTT_Client*)args;
	INFO("MQTT: Published\r\n");
}

void ICACHE_FLASH_ATTR
mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
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

void ICACHE_FLASH_ATTR
initiateCaliperSensorSamplingTimer(void) {
  // This will initiate a callback to caliperdialSensorDataCb when results are available
  int nextPeriod = SENSOR_SAMPLE_PERIOD;
  if (broker_established) {
    if ( !caliper_startSampling() ) {
      nextPeriod = SENSOR_SAMPLE_PERIOD/2;
      os_printf("Caliper sensor is still running, tmp result is:\n");
      GPIOI_debugTrace(-1,-24);
    }
  }
  os_timer_disarm(&sensor_timer);
  os_timer_arm(&sensor_timer, nextPeriod, 0);
}

void ICACHE_FLASH_ATTR
caliperSensorDataCb(void) {
  if (broker_established) {
    int bytesWritten = 0;
    if (caliper_readAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten)) {
      INFO("MQTT caliperSensorDataCb: received %s\r\n", sendbuffer);
      MQTT_Publish( &mqttClient, clientid, sendbuffer, bytesWritten, 0, false);
      MQTT_Publish( &mqttClient, "/939029e", sendbuffer, bytesWritten, 0, false);
    }
  }
}

void ICACHE_FLASH_ATTR
initiateDialSensorSamplingTimer(void) {
  // This will initiate a callback to dialSensorDataCb when results are available
  int nextPeriod = SENSOR_SAMPLE_PERIOD;
  if (broker_established) {
    if ( !dial_startSampling() ) {
      nextPeriod = SENSOR_SAMPLE_PERIOD/2;
      os_printf("Dial sensor is still running, tmp result is:\n");
      GPIOI_debugTrace(-1,-24);
    }
  }
  os_timer_disarm(&sensor_timer);
  os_timer_arm(&sensor_timer, nextPeriod, 0);
}

void ICACHE_FLASH_ATTR
dialSensorDataCb(void) {
  if (broker_established) {
    int bytesWritten = 0;
    if (dial_readAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten)) {
      INFO("MQTT dialSensorDataCb: received %s\r\n", sendbuffer);
      MQTT_Publish( &mqttClient, clientid, sendbuffer, bytesWritten, 0, false);
      MQTT_Publish( &mqttClient, "/939029e", sendbuffer, bytesWritten, 0, false);
    }
  }
}

void ICACHE_FLASH_ATTR
initiateWattSensorSamplingTimer(void) {
  // This will initiate a callback to dialSensorDataCb when results are available
  int nextPeriod = SENSOR_SAMPLE_PERIOD;
  if (broker_established) {
    if ( !watt_startSampling() ) {
      nextPeriod = SENSOR_SAMPLE_PERIOD/2;
      os_printf("Watt sensor is still running, tmp result is:\n");
      GPIOI_debugTrace(0,23);
    }
  }
  os_timer_disarm(&sensor_timer);
  os_timer_arm(&sensor_timer, nextPeriod, 0);
}

void ICACHE_FLASH_ATTR
wattSensorDataCb(void) {
  if (broker_established) {
    int bytesWritten = 0;
    if (watt_readAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten)) {
      INFO("MQTT wattSensorDataCb: received %s\r\n", sendbuffer);
      MQTT_Publish( &mqttClient, clientid, sendbuffer, bytesWritten, 0, false);
    }
  }
}

void ICACHE_FLASH_ATTR
performMaxSensorSamplingTimer(void) {
  // This does not initiate a callback. The sampling is synchronous
  int nextPeriod = SENSOR_SAMPLE_PERIOD;
  if (broker_established) {
    int bytesWritten = 0;
    if (max6675_readTempAsString(sendbuffer, SENDBUFFERSIZE, &bytesWritten, true)) {
      INFO("MQTT performMaxSensorSamplingTimer: received %s\r\n", sendbuffer);
      MQTT_Publish( &mqttClient, clientid, sendbuffer, bytesWritten, 0, false);
    } else {
      nextPeriod = SENSOR_SAMPLE_PERIOD/2;
    }
  }
  os_timer_disarm(&sensor_timer);
  os_timer_arm(&sensor_timer, nextPeriod, 0);
}


void user_init(void)
{
  // Make os_printf working again. Baud:115200,n,8,1
  stdoutInit();
  os_timer_disarm(&sensor_timer);
	os_delay_us(400000);
	INFO("\r\nStarting\r\n");

	CFG_Load();
	gpio_init();
#ifdef USE_DIAL_SENSOR
	dial_init((os_timer_func_t*) dialSensorDataCb);
#endif
#ifdef USE_CALIPER_SENSOR
	caliper_init((os_timer_func_t*) caliperSensorDataCb);
#endif
#ifdef USE_MAX75_SENSOR
	max6675_init();
#endif
#ifdef USE_WATT_SENSOR
  watt_init((os_timer_func_t*) wattSensorDataCb);
#endif
	MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, SEC_NONSSL);
	MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive);
	MQTT_OnConnected(&mqttClient, mqttConnectedCb);
	MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
	MQTT_OnPublished(&mqttClient, mqttPublishedCb);
	MQTT_OnData(&mqttClient, mqttDataCb);

	INFO("\r\nConnecting to SSID:");INFO(sysCfg.sta_ssid);INFO("\r\n");
	WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

	INFO("\r\nSystem started ...\r\n");

  //Setup timer
#ifdef USE_DIAL_SENSOR
  os_timer_setfn(&sensor_timer, (os_timer_func_t*) initiateDialSensorSamplingTimer, NULL);
#endif
#ifdef USE_CALIPER_SENSOR
  os_timer_setfn(&sensor_timer, (os_timer_func_t*) initiateCaliperSensorSamplingTimer, NULL);
#endif
#ifdef USE_MAX75_SENSOR
  os_timer_setfn(&sensor_timer, (os_timer_func_t*) performMaxSensorSamplingTimer, NULL);
#endif
#ifdef USE_WATT_SENSOR
  os_timer_setfn(&sensor_timer, (os_timer_func_t*) initiateWattSensorSamplingTimer, NULL);
#endif
  os_timer_arm(&sensor_timer, SENSOR_SAMPLE_PERIOD, 0);
}
