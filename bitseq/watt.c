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
#include "bitseq/gpio_intr.h"

static os_timer_func_t *userCallback = NULL;

bool ICACHE_FLASH_ATTR
watt_read(float *sample)
{
  if (userCallback==NULL) {
    os_printf("Error read watt: call watt_init first!\n\r");
    return false;
  }

  if ( GPIOI_hasResults() ) {
    int32_t result;
    //uint32_t tmp;
    //uint32_t byte = 0;

    result = GPIOI_sliceBits(-24, -1, true)<<1;
    //os_printf("GPIOI got result: ");
    //GPIOI_debugTrace(-112, -1);
    //tmp = GPIOI_sliceBits(-24,-1, true);
    //os_printf("\nword -1: %d\n", tmp);
    //tmp = GPIOI_sliceBits(-56,-33, true);
    //os_printf("word -2: %d\n", tmp);
    //tmp = GPIOI_sliceBits(-88,-65, true);
    //os_printf("word -3: %d\n", tmp);

    *sample = result;
    return true;
  } else {
    os_printf("GPIOI Still running, tmp result is: ");
    GPIOI_debugTrace(-112,-1);
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
    *bytesWritten = GPIOI_float_2_string(1.0f*sample, 1000, buf, bufLen);
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
watt_init(os_timer_func_t *resultCb) {

  // Acquire 64 bits
  // at least 40 ms between blocks
  // rising edge

  GPIOI_init(64, 40000, true, resultCb);
  userCallback = resultCb;
}

