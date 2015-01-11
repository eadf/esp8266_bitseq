/*
 * dial.c
 *
 *  Created on: Jan 2, 2015
 *      Author: ead fritz
 */
#include "driver/dial.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "driver/dro_utils.h"
#include "driver/gpio_intr.h"

static const float CONVERT_TO_MM = 1.2397707131274277f;
static os_timer_func_t *userCallback = NULL;

bool ICACHE_FLASH_ATTR
readDial(float *sample)
{
  if (userCallback==NULL) {
    os_printf("Error readDial: call dialInit first!\n\r");
    return false;
  }

  if ( GPIOI_hasResults() ) {
    uint32_t result;
    int32_t polishedResult;

    result = GPIOI_sliceBits(0,23);
    polishedResult = result;
    if (result & 1<<23 ) {
      // bit 23 indicates negative value, just set bit 24-31 as well
      polishedResult = (int32_t)(CONVERT_TO_MM*((int32_t)(result|0xff000000)));
    } else {
      polishedResult = (int32_t)(CONVERT_TO_MM*result);
    }
    os_printf("GPIOI got result: ");
    GPIOI_debugTrace(0,23);
    *sample = 0.0001f*polishedResult;
    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is: ");
    GPIOI_debugTrace(0,23);
  }
  return false;
}

bool ICACHE_FLASH_ATTR
startDialSample(void) {
  if (!GPIOI_isRunning()){
    //os_printf("Setting new interrupt handler\n\r");
    GPIOI_enableInterrupt();
    return true;
  } else {
    return false;
  }
}

bool ICACHE_FLASH_ATTR
readDialAsString(char *buf, int bufLen, int *bytesWritten) {
  float sample = 0.0;
  bool rv = readDial(&sample);
  if(rv){
    *bytesWritten = dro_utils_float_2_string(10000.0f*sample, 10000, buf, bufLen);
    // the unit is always sent as mm from the dial, regardless of the inch/mm button
    if(bufLen > *bytesWritten + 4) {
      buf[*bytesWritten+0] = 'm';
      buf[*bytesWritten+1] = 'm';
      buf[*bytesWritten+2] = 0;
      *bytesWritten = *bytesWritten+3;
    }
  } else {
    *bytesWritten = 0;
    buf[0] = '\0';
  }
  return rv;
}

bool ICACHE_FLASH_ATTR
isDialIdle(void) {
  return GPIOI_isIdle();
}

/**
 * Setup the hardware and initiate callbacks
 */
void ICACHE_FLASH_ATTR
dialInit(os_timer_func_t *resultCb) {

  // Acquire 24 bits
  // at most 18 us between clock pulses
  // at least 70 us between blocks
  // at most 100 us between blocks
  // falling edge
  GPIOI_init(24, 18, 70, 100, false, resultCb);
  userCallback = resultCb;
}

