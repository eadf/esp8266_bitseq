#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_interface.h"
#include "driver/stdout.h"
#include "bitseq/dial.h"
#include "bitseq/caliper.h"
#include "bitseq/watt.h"
#include "bitseq/gpio_intr.h"
#include "user_config.h"

#define user_procTaskPrio        0
#define user_procTaskQueueLen    1
os_event_t user_procTaskQueue[user_procTaskQueueLen];
static void user_procTask(os_event_t *events);

static volatile os_timer_t sensor_timer;

#define CHRBUFFERSIZE 128
static char chrBuffer[CHRBUFFERSIZE];

#ifdef USE_DIAL_SENSOR
#undef USE_CALIPER_SENSOR
#undef USE_WATT_SENSOR
void ICACHE_FLASH_ATTR
initiateDialSensorSamplingTimer(void) {
  if ( !dial_startSampling() ) {
    os_printf("Dial sensor is still running, tmp result is:\n");
    bitseq_debugTrace(-1,-24);
  }
}

void ICACHE_FLASH_ATTR
dialSensorDataCb(void) {
  int bytesWritten = 0;
  if (dial_readAsString(chrBuffer, CHRBUFFERSIZE, &bytesWritten)) {
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
    bitseq_debugTrace(-1,-24);
  }
}

void ICACHE_FLASH_ATTR
caliperSensorDataCb(void) {
  int bytesWritten = 0;
  if (caliper_readAsString(chrBuffer, CHRBUFFERSIZE, &bytesWritten)) {
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
    bitseq_debugTrace(-1,-24);
  }
}

void ICACHE_FLASH_ATTR
wattSensorDataCb(void) {
  int bytesWritten = 0;
  if (watt_readAsString(chrBuffer, CHRBUFFERSIZE, &bytesWritten)) {
    os_printf("wattSensorDataCb: received %s\r\n", chrBuffer);
  }
}
#endif

//Do nothing function
static void ICACHE_FLASH_ATTR
user_procTask(os_event_t *events) {
  os_delay_us(10);
}

//Init function
void ICACHE_FLASH_ATTR
user_init() {
  // Make os_printf working again. Baud:115200,n,8,1
  stdoutInit();

  //Set station mode
  wifi_set_opmode( NULL_MODE );

  // Initialize the GPIO subsystem.
  gpio_init();

  //Disarm timer
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
  watt_init((os_timer_func_t*) wattSensorDataCb);
  os_timer_setfn(&sensor_timer, (os_timer_func_t *) initiateWattSensorSamplingTimer, NULL);
#endif

  //Arm the timer
  os_timer_arm(&sensor_timer, SENSOR_SAMPLE_PERIOD, 1);

  //Start os task
  system_os_task(user_procTask, user_procTaskPrio, user_procTaskQueue, user_procTaskQueueLen);
}
