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

const int CALIPER_CLOCK_IN_PIN = 0;
const int CALIPER_DATA_IN_PIN = 2;

/**
 * The original arduino source code was made for an inverting level shifter.
 * I'm using non-inverting hardware, so the program logic has to be inverted back again :/
 */
bool caliperDigitalValue(int pin) {
  return GPIO_INPUT_GET(pin) != 1;
}

bool ICACHE_FLASH_ATTR caliperDecode_BITBANG(float *returnValue) {
  int sign=1;
  long value=0;
  int i=0;
  for (i=0;i<23;i++) {
    while (caliperDigitalValue(CALIPER_CLOCK_IN_PIN)) { os_delay_us(1); } //wait until clock returns to HIGH- the first bit is not needed
    while (!caliperDigitalValue(CALIPER_CLOCK_IN_PIN)) { os_delay_us(1);} //wait until clock returns to LOW
    if (!caliperDigitalValue(CALIPER_DATA_IN_PIN)) {
      if (i<20) {
        value |= 1<<i;
      }
      if (i==20) {
        sign=-1;
      }
    }
  }
  *returnValue = (float)(value*sign)/100.0;

  return true;
}

/**
 * Will set the input pin to inputs.
 */
void ICACHE_FLASH_ATTR caliperInit() {
  //os_printf("Caliper: Setting pins as input\r\n");

  //set gpio2 as gpio pin
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
  //set gpio0 as gpio pin
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);

  //disable pull downs
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO2_U);
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO0_U);

  //disable pull ups
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);

  GPIO_DIS_OUTPUT(CALIPER_CLOCK_IN_PIN);
  GPIO_DIS_OUTPUT(CALIPER_DATA_IN_PIN);
}

bool
ICACHE_FLASH_ATTR ICACHE_FLASH_ATTR readCaliper_BITBANG(float *sample)
{
  uint32 i = 0;

  // just to be safe - set pins as input once again
  caliperInit();

  //if clock is LOW wait until it turns to HIGH
  for (i=0; caliperDigitalValue(CALIPER_CLOCK_IN_PIN); ) {
    os_delay_us(1);
    i++;
    if(i==100000){
      //os_printf("Caliper: No Data 1\r\n");
      return false;
    }
  }
  uint32 tempmicros = dro_utils_micros();

  //wait for the end of the HIGH pulse
  for (i=0; !caliperDigitalValue(CALIPER_CLOCK_IN_PIN); ) {
    os_delay_us(1);
    i++;
    if(i==100000){
      //os_printf("Caliper: No Data 2\r\n");
      return false;
    }
  }

  uint32 duration = dro_utils_micros()-tempmicros;
  if (duration>500) { //if the HIGH pulse was longer than 500 micros we are at the start of a new bit sequence
    caliperDecode_BITBANG(sample); //caliperDecode the bit sequence
    //os_printf("started scanning and found something %d us later. Found %d\r\n" , duration, (int)(100* *sample));
    return true;
  } else {
    os_printf("Caliper: started scanning and gave up %d us later.\r\n" , duration);
    return false;
  }
}

bool ICACHE_FLASH_ATTR readCaliperAsString(char *buf, int bufLen, int *bytesWritten){
  float sample = 0.0;
  bool rv = readCaliper_BITBANG(&sample);
  if(rv){
    *bytesWritten = dro_utils_float_2_string(100.0*sample, 100, buf, bufLen);
  } else {
    *bytesWritten = 0;
    buf[0] = 0;
  }
  return rv;
}
