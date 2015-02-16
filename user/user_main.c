#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "stdout/stdout.h"
#include "bitseq/dial.h"
#include "bitseq/caliper.h"
#include "bitseq/watt.h"
#include "bitseq/bitseq.h"
#include "user_config.h"


#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);
static void setup(void);

static volatile os_timer_t sensor_timer;

#define CHR_BUFFER_SIZE 128
static char chrBuffer[CHR_BUFFER_SIZE];

#ifdef USE_DIAL_SENSOR
#undef USE_CALIPER_SENSOR
#undef USE_WATT_SENSOR
void ICACHE_FLASH_ATTR
initiateDialSensorSamplingTimer(void) {
  if ( !dial_startSampling() ) {
    os_printf("Dial sensor is still running, tmp result is:\n");
    bitseq_debugTrace(-24,-1);
  }
}

void ICACHE_FLASH_ATTR
dialSensorDataCb(void) {
  int bytesWritten = 0;
  if (dial_readAsString(chrBuffer, CHR_BUFFER_SIZE, &bytesWritten)) {
    os_printf("dialSensorDataCb: received %s\r\n", chrBuffer);
  }
}
#endif

#ifdef USE_CALIPER_SENSOR
#undef USE_DIAL_SENSOR
#undef USE_WATT_SENSOR
void ICACHE_FLASH_ATTR
initiateCaliperSensorSamplingTimer(void) {
  if ( !caliper_startSampling() ) {
    os_printf("Caliper sensor is still running, tmp result is:\n");
    bitseq_debugTrace(-24,-1);
  }
}

void ICACHE_FLASH_ATTR
caliperSensorDataCb(void) {
  int bytesWritten = 0;
  if (caliper_readAsString(chrBuffer, CHR_BUFFER_SIZE, &bytesWritten)) {
    os_printf("caliperSensorDataCb: received %s\r\n", chrBuffer);
  }
}
#endif

#ifdef USE_WATT_SENSOR
#undef USE_DIAL_SENSOR
#undef USE_CALIPER_SENSOR
void ICACHE_FLASH_ATTR
initiateWattSensorSamplingTimer(void) {
  if ( !watt_startSampling() ) {
    os_printf("Watt sensor is still running, tmp result is:\n");
    bitseq_debugTrace(-24,-1);
  }
}

void ICACHE_FLASH_ATTR
wattSensorDataCb(void) {
  int bytesWritten = 0;
  if (watt_readAsString(chrBuffer, CHR_BUFFER_SIZE, &bytesWritten)) {
    os_printf("wattSensorDataCb: received %s\r\n", chrBuffer);
  }
}
#endif

/**
 * Setup program. When user_init runs the debug printouts will not always
 * show on the serial console. So i run the inits in here, 2 seconds later.
 */
static void ICACHE_FLASH_ATTR
setup(void) {
  os_timer_disarm(&sensor_timer);

  //Setup timer
#ifdef USE_DIAL_SENSOR
  dial_init(false, (os_timer_func_t*) dialSensorDataCb);
  os_timer_setfn(&sensor_timer, (os_timer_func_t *) initiateDialSensorSamplingTimer, NULL);
#endif
#ifdef USE_CALIPER_SENSOR
  caliper_init(false, (os_timer_func_t*) caliperSensorDataCb);
  os_timer_setfn(&sensor_timer, (os_timer_func_t *) initiateCaliperSensorSamplingTimer, NULL);
#endif
#ifdef USE_WATT_SENSOR
  watt_init(false, (os_timer_func_t*) wattSensorDataCb);
  os_timer_setfn(&sensor_timer, (os_timer_func_t *) initiateWattSensorSamplingTimer, NULL);
#endif

  //Arm the timer
  os_timer_arm(&sensor_timer, SENSOR_SAMPLE_PERIOD, true);
}

//Do nothing function
static void ICACHE_FLASH_ATTR
user_procTask(os_event_t *events) {
  os_delay_us(10);
}

//Init function
void ICACHE_FLASH_ATTR
user_init() {
  // Initialize the GPIO subsystem.
  gpio_init();

  // Make uart0 work with just the TX pin. Baud:115200,n,8,1
  // The RX pin is now free for GPIO use.
  stdout_init();

  // turn off WiFi for this console only demo
  wifi_station_set_auto_connect(false);
  wifi_station_disconnect();

  //Set the setup timer
  os_timer_disarm(&sensor_timer);
  os_timer_setfn(&sensor_timer, (os_timer_func_t *) setup, NULL);
  os_timer_arm(&sensor_timer, SENSOR_SAMPLE_PERIOD, false);

  //Start os task
  system_os_task(user_procTask, user_procTaskPrio, user_procTaskQueue, user_procTaskQueueLen);
}
