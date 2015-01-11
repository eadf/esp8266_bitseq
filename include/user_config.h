#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_
#include "user_interface.h"


// ****************  IMPORTANT *********************** 
// Whenever you change anything in this file you will have to change/increment the value of 
// CFG_HOLDER or your changes will not be used the next time you flash your chip
#define CFG_HOLDER	0x00FF55A5


#define CFG_LOCATION	0x3C	/* Please don't change or if you know what you doing */

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST		"192.168.0.10"
#define MQTT_PORT		1883

#define MQTT_BUF_SIZE		1024
#define MQTT_KEEPALIVE		120

#define MQTT_CLIENT_ID		"EPS_%08X"
#define MQTT_USER		"eps-user"
#define MQTT_PASS		"eps-pass"

#define STA_SSID ""
#define STA_PASS ""
#define STA_TYPE AUTH_WPA2_PSK
#define MQTT_RECONNECT_TIMEOUT 	5
#define MQTT_CONNTECT_TIMER 	5
#define SENSOR_SAMPLE_PERIOD 1000

// only one of USE_DIAL_SENSOR, USE_CALIPER_SENSOR or USE_MAX75_SENSOR can be active at a time
#define USE_DIAL_SENSOR
//#define USE_CALIPER_SENSOR
//#define USE_MAX75_SENSOR
#endif
