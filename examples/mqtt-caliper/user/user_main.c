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
#include "driver/stdout.h"
#include "osapi.h"
#include "mqtt.h"
#include "wifi.h"
#include "config.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "bitseq/caliper.h"
#include "bitseq/bitseq.h"
#include "user_config.h"

#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"

#define SENSOR_BUFFER_SIZE 128
static char sensorBuffer[SENSOR_BUFFER_SIZE];

static MQTT_Client mqttClient;
static volatile os_timer_t sensor_timer;
static char clientid[66];  // the MAC address


void ICACHE_FLASH_ATTR
initiateCaliperSensorSamplingTimer(void) {
  // This will initiate a callback to caliperdialSensorDataCb when results are available
  int nextPeriod = SENSOR_SAMPLE_PERIOD;
  if ( !caliper_startSampling() ) {
    nextPeriod = SENSOR_SAMPLE_PERIOD/2;
    os_printf("Caliper sensor is still running, tmp result is:\n");
    bitseq_debugTrace(-1,-24);
  } else {
    //os_printf("Initated a new sample");
  }
  os_timer_disarm(&sensor_timer);
  os_timer_arm(&sensor_timer, nextPeriod, 0);
}

void ICACHE_FLASH_ATTR
caliperSensorDataCb(void) {
  int bytesWritten = 0;
  if (caliper_readAsString(sensorBuffer, SENSOR_BUFFER_SIZE, &bytesWritten)) {
    INFO("MQTT caliperSensorDataCb: received %s\r\n", sensorBuffer);
    MQTT_Publish( &mqttClient, clientid, sensorBuffer, bytesWritten, 0, false);
    // pad the text with ' ' so that it fills an entire line
    for (;bytesWritten<12; bytesWritten++) {
      sensorBuffer[bytesWritten] = ' ';
      sensorBuffer[bytesWritten+1] = 0;
    }
    //os_printf("Sending >%s< to lcd. %d\n", sensorBuffer, bytesWritten);
    MQTT_Publish( &mqttClient, "/lcd3", sensorBuffer, bytesWritten, 0, false);
  }
}


void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status) {
  if (status == STATION_GOT_IP) {
    char hwaddr[6];
    wifi_get_macaddr(0, hwaddr);
    os_sprintf(clientid, "/" MACSTR , MAC2STR(hwaddr));

    MQTT_Connect(&mqttClient);
  }
}

void ICACHE_FLASH_ATTR
mqttConnectedCb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*) args;
  INFO("MQTT: Connected\r\n");
  MQTT_Publish( &mqttClient, "/lcd/clearscreen", "", 0, 0, false);
  MQTT_Publish( &mqttClient, "/lcd2", "  Caliper:  ", 12, 0, false);
  // now when we got a mqtt broker connection - start sampling
  os_timer_disarm(&sensor_timer);
  os_timer_setfn(&sensor_timer, (os_timer_func_t*) initiateCaliperSensorSamplingTimer, NULL);
  os_timer_arm(&sensor_timer, SENSOR_SAMPLE_PERIOD, 0);
}

void ICACHE_FLASH_ATTR
mqttDisconnectedCb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*) args;
  INFO("MQTT: Disconnected\r\n");
}

void ICACHE_FLASH_ATTR
mqttPublishedCb(uint32_t *args) {
  MQTT_Client* client = (MQTT_Client*) args;
  INFO("MQTT: Published\r\n");
}

void ICACHE_FLASH_ATTR
mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
  char *topicBuf = (char*) os_zalloc(topic_len + 1);
  char *dataBuf = (char*) os_zalloc(data_len + 1);

  MQTT_Client* client = (MQTT_Client*) args;

  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;

  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;

  INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}



void ICACHE_FLASH_ATTR
user_init(void) {
  // Make os_printf working again. Baud:115200,n,8,1
  stdoutInit();
  os_delay_us(1000000);

  CFG_Load();
  gpio_init();
  caliper_init(false, (os_timer_func_t*) caliperSensorDataCb);

  MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
  MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
  MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);
  WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifiConnectCb);

  INFO("\r\nSystem started ...\r\n");
}
