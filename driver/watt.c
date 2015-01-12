/*
 * watt.c
 *
 *  Created on: Jan 2, 2015
 *      Author: ead fritz
 */
#include "driver/watt.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "driver/dro_utils.h"
#include "driver/gpio_intr.h"

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

    result = GPIOI_sliceBits(529,559);
    os_printf("GPIOI got result: ");
    GPIOI_debugTrace(529,559);
    *sample = result;
    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is: ");
    GPIOI_debugTrace(529,559);
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
    *bytesWritten = dro_utils_float_2_string(10000.0f*sample, 10000, buf, bufLen);
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
  // I'm cutting it close with the timing limits because
  // the watt sends two 24 bit blocks and we are only interested in the last one
  // The blocks are separated by 85 us or so.

  // Acquire 560 bits
  // at most 2000 us between clock pulses
  // at least 40 ms between blocks
  // at most 1s us between blocks
  // rising edge
  GPIOI_init(560, 2000, 44000, 1000000, true, resultCb);
  userCallback = resultCb;
}

