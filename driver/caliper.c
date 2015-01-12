/*
 * caliper.c
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */
#include "driver/caliper.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "driver/dro_utils.h"

static const float CONVERT_TO_MM = 0.01f;
static const float CONVERT_TO_INCH = 0.0005f;

static os_timer_func_t *userCallback = NULL;

bool ICACHE_FLASH_ATTR
caliper_startSampling(void) {
  if (!GPIOI_isRunning()){
    //os_printf("caliper_startSampling: Setting new interrupt handler\n\r");
    GPIOI_enableInterrupt();
    return true;
  } else {
    return false;
  }
}


bool ICACHE_FLASH_ATTR
caliper_read(float *sample, bool *isMM) {

  if (userCallback==NULL) {
    os_printf("Error caliper_read: call caliperInit first!\n\r");
    return false;
  }

  if ( GPIOI_hasResults() ) {
    int32_t result = GPIOI_sliceBits(0,23);

    if(result & 1<<23){
      *isMM = false;
      // lower bit 23 again
      result &= ~(1<<23);
    } else {
      *isMM = true;
    }
    if (result & 1<<20 ) {
      // bit 20 signals negative value
      result &= ~(1<<20); // lower bit 20 again
      result = -result; // set it to negative value
    }

    os_printf("Result is %d : ", result);
    GPIOI_debugTrace(0,23);
    if (*isMM){
      *sample = CONVERT_TO_MM * result;
    } else {
      *sample = CONVERT_TO_INCH * result;
    }
    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is:\n");
    GPIOI_debugTrace(0,23);
    return false;
  }
}


bool ICACHE_FLASH_ATTR
caliper_readAsString(char *buf, int bufLen, int *bytesWritten){
  float sample = 0.0;
  bool isMM = true;
  bool rv = caliper_read(&sample, &isMM);
  if(rv){
    if(isMM){
      *bytesWritten = dro_utils_float_2_string(100.0f*sample, 100, buf, bufLen);
    } else {
      *bytesWritten = dro_utils_float_2_string(10000.0f*sample, 10000, buf, bufLen);
    }
    if(bufLen > *bytesWritten + 4) {
      if(isMM){
        buf[*bytesWritten+0] = 'm';
        buf[*bytesWritten+1] = 'm';
        buf[*bytesWritten+2] = 0;
        *bytesWritten = *bytesWritten+3;
      } else {
        buf[*bytesWritten+0] = '"';
        buf[*bytesWritten+1] = 0;
        *bytesWritten = *bytesWritten+2;
      }
    }
  } else {
    *bytesWritten = 0;
    buf[0] = 0;
  }
  return rv;
}

/**
 * Will set the input pin to inputs.
 */
void ICACHE_FLASH_ATTR
caliper_init(os_timer_func_t *resultCb) {
  // Acquire 24 bits
  // at most 900 us between clock pulses
  // at least 10 ms between blocks
  // at most 5 sec between blocks
  // rising edge

  GPIOI_init(24, 900, 10000, 5000000, true, resultCb);
  userCallback = resultCb;
}
