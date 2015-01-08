#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_
#include "user_interface.h"


// ****************  IMPORTANT *********************** 
// Whenever you change anything in this file you will have to change/increment the value of 
// CFG_HOLDER or your changes will not be used the next time you flash your chip (for some reason)
#define CFG_HOLDER	0x00FF55A5


#define CFG_LOCATION	0x3C	/* Please don't change or if you know what you doing */

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST		"192.168.0.10"
#define MQTT_PORT		8080

#define MQTT_BUF_SIZE		1024
#define MQTT_KEEPALIVE		120

#define MQTT_CLIENT_ID		"DVES_%08X"
#define MQTT_USER		"dro-user"
#define MQTT_PASS		"dro-pass"

#define STA_SSID "DlinkWLAN"
#define STA_PASS "cx7YaZ5b1"
#define STA_TYPE AUTH_WPA2_PSK
#define MQTT_RECONNECT_TIMEOUT 	5
#define MQTT_CONNTECT_TIMER 	5
#define CALIPER_SAMPLE_PERIOD 500

#endif
