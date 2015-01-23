/*
 * caliper.c
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */
#include "bitseq/caliper.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "bitseq/gpio_intr.h"

static const float CONVERT_TO_MM = 0.01f;
static const float CONVERT_TO_INCH = 0.0005f;

static os_timer_func_t *userCallback = NULL;
static bool caliper_negativeLogic = false;

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
    int32_t result = GPIOI_sliceBits(-1,-24,false);
    if (caliper_negativeLogic) {
      result =  (~result) & 0x00ffffff;
    }
    //os_printf("\n");
    //GPIOI_printBinary32(result);
    //os_printf("\n");
    // bit 23 indicates inches
    if(result & 1<<23){
      *isMM = false;
      // lower bit 23 again
      result &= ~(1<<23);
      *sample = CONVERT_TO_INCH;
    } else {
      *isMM = true;
      *sample = CONVERT_TO_MM;
    }
    if (result & 1<<20 ) {
      // bit 20 signals negative value
      result &= ~(1<<20); // lower bit 20 again
      *sample = -*sample;
    }

    os_printf("Result is %d : ", result);
    GPIOI_debugTrace(-1,-24);
    *sample = *sample * result;

    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is:\n");
    GPIOI_debugTrace(-1,-25);
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
      *bytesWritten = GPIOI_float_2_string(100.0f*sample, 100, buf, bufLen);
    } else {
      *bytesWritten = GPIOI_float_2_string(10000.0f*sample, 10000, buf, bufLen);
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
caliper_init(bool negativeLogic, os_timer_func_t *resultCb) {
  caliper_negativeLogic = negativeLogic;
  // Acquire 24 bits
  // at least 10 ms between blocks
  // when a non-inverting amplifier is used we should trigger on rising edge
  GPIOI_init(24, 10000, !negativeLogic, resultCb);
  userCallback = resultCb;
}
