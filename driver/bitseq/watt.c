/*
 * watt.c
 *
 *  Created on: Jan 2, 2015
 *      Author: eadf
 */
#include "bitseq/watt.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "bitseq/bitseq.h"

static os_timer_func_t *watt_userCallback = NULL;
static bool watt_negativeLogic = false;

bool ICACHE_FLASH_ATTR
watt_read(float *sample)
{
  if (watt_userCallback==NULL) {
    os_printf("Error read watt: call watt_init first!\n\r");
    return false;
  }

  if ( bitseq_hasResults() ) {
    //uint32_t tmp;
    //uint32_t byte = 0;

    uint32_t raw  = bitseq_sliceBits(-24, -1, true)<<1;
    if (watt_negativeLogic) {
      raw = ~raw;
    }
    *sample = (int32_t) raw; // convert to int32_t then to float

    //os_printf("BITSEQ got signedResult: ");
    //bitseq_debugTrace(-112, -1);
    //tmp = bitseq_sliceBits(-24,-1, true);
    //os_printf("\nword -1: %d\n", tmp);
    //tmp = bitseq_sliceBits(-56,-33, true);
    //os_printf("word -2: %d\n", tmp);
    //tmp = bitseq_sliceBits(-88,-65, true);
    //os_printf("word -3: %d\n", tmp);

    return true;
  } else {
    os_printf("BITSEQ sampler still running, tmp signedResult is: ");
    bitseq_debugTrace(-112,-1);
  }
  return false;
}

bool ICACHE_FLASH_ATTR
watt_startSampling(void) {
  if (!bitseq_isRunning()){
    //os_printf("Setting new interrupt handler\n\r");
    bitseq_enableInterrupt();
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
    *bytesWritten = bitseq_float2string(1.0f*sample, 1000, buf, bufLen);
    if (bufLen > *bytesWritten+1) {
      buf[*bytesWritten] = 'W';
      buf[*bytesWritten+1] = 0;
      *bytesWritten += 1;
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
watt_init(bool negativeLogic, os_timer_func_t *resultCb) {
  watt_negativeLogic = negativeLogic;

  // Acquire 64 bits ( we only need 24 bits of the 4480 bits the device transmit every 900ms)
  // at least 40 ms between blocks
  // rising edge (on non-inverting logic)

  bitseq_init(64, 40000, !watt_negativeLogic, resultCb);
  watt_userCallback = resultCb;
}

