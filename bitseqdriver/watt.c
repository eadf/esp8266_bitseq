/*
 * watt.c
 *
 *  Created on: Jan 2, 2015
 *      Author: ead fritz
 */
#include "bitseqdriver/watt.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "bitseqdriver/gpio_intr.h"

static os_timer_func_t *userCallback = NULL;

bool ICACHE_FLASH_ATTR
watt_read(float *sample)
{
  if (userCallback==NULL) {
    os_printf("Error read watt: call watt_init first!\n\r");
    return false;
  }

  if ( GPIOI_hasResults() ) {
    uint32_t result;

    result = GPIOI_sliceBits(-1,-32, false);
    os_printf("GPIOI got result: ");
    GPIOI_debugTrace(-1,-32);
    *sample = result;
    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is: ");
    GPIOI_debugTrace(-1,-32);
  }
  return false;
}

bool ICACHE_FLASH_ATTR
watt_startSampling(void) {
  if (!GPIOI_isRunning()){
    //os_printf("Setting new interrupt handler\n\r");
    GPIOI_enableInterrupt();
    return true;
  } else {
    return false;
  }
}

bool ICACHE_FLASH_ATTR
watt_readAsString(char *buf, int bufLen, int *bytesWritten) {
  float sample = 0.0;
  bool rv = watt_read(&sample);
  if(rv){
    *bytesWritten = GPIOI_float_2_string(10000.0f*sample, 10000, buf, bufLen);
  } else {
    *bytesWritten = 0;
    buf[0] = '\0';
  }
  return rv;
}

/*bool ICACHE_FLASH_ATTR
watt_isIdle(void) {
  return GPIOI_isIdle();
}*/

/**
 * Setup the hardware and initiate callbacks
 */
void ICACHE_FLASH_ATTR
watt_init(os_timer_func_t *resultCb) {

  // Acquire 64 bits
  // at least 40 ms between blocks
  // rising edge

  GPIOI_init(64, 40000, true, resultCb);
  userCallback = resultCb;
}

