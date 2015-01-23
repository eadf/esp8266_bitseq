/*
 * dial.c
 *
 *  Created on: Jan 2, 2015
 *      Author: ead fritz
 */
#include "bitseq/dial.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "bitseq/bitseq.h"

static const float CONVERT_TO_MM = 1.2397707131274277f;
static os_timer_func_t *userCallback = NULL;
static bool dial_negativeLogic = false;

bool ICACHE_FLASH_ATTR
dial_read(float *sample)
{
  if (userCallback==NULL) {
    os_printf("Error readDial: call dialInit first!\n\r");
    return false;
  }

  if ( bitseq_hasResults() ) {
    uint32_t result;
    int32_t polishedResult;

    result = bitseq_sliceBits(-2,-25,true);
    if (dial_negativeLogic) {
      result = ~result;
    }
    polishedResult = result;
    if (result & 1<<23) {
      // bit 23 indicates negative value, just set bit 24-31 as well
      polishedResult = (int32_t)(CONVERT_TO_MM*((int32_t)(result|0xff000000)));
    } else {
      polishedResult = (int32_t)(CONVERT_TO_MM*result);
    }
    //os_printf("GPIOI got result: ");
    //bitseq_debugTrace(-2,-25);
    *sample = 0.0001f*polishedResult;
    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is: ");
    bitseq_debugTrace(-2,-25);
  }
  return false;
}

bool ICACHE_FLASH_ATTR
dial_startSampling(void) {
  if (!bitseq_isRunning()){
    //os_printf("Setting new interrupt handler\n\r");
    bitseq_enableInterrupt();
    return true;
  } else {
    return false;
  }
}

bool ICACHE_FLASH_ATTR
dial_readAsString(char *buf, int bufLen, int *bytesWritten) {
  float sample = 0.0;
  bool rv = dial_read(&sample);
  if(rv){
    *bytesWritten = bitseq_float2string(10000.0f*sample, 10000, buf, bufLen);
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

/**
 * Setup the hardware and initiate callbacks
 */
void ICACHE_FLASH_ATTR
dial_init(bool negativeLogic, os_timer_func_t *resultCb) {
  dial_negativeLogic = negativeLogic;
  // Acquire 48 bits
  // at least 90 ms between blocks
  // falling edge on non-inverting logic
  bitseq_init(48, 90000, dial_negativeLogic, resultCb);
  userCallback = resultCb;
}

