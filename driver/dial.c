/*
 * dial.c
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */
#include "driver/dial.h"
#include "eagle_soc.h" // gpio.h requires this, why can't it include it itself?
#include "gpio.h"
#include "osapi.h"
#include "driver/dro_utils.h"

const int CLOCK_IN_PIN = 0;
const int DATA_IN_PIN = 2;

/**
 * I've made this a 'one stop' function for pin access. So if you need to invert the logic
 * you can do it here, in one single place.
 */
bool dialDigitalValue(int pin) {
  return GPIO_INPUT_GET(pin) == 1;
}

bool dialDecode(float *returnValue) {
  //char samples[30];
  //uint32 duration = 0;
  //uint32 tempmicros = 0;
  //uint32 longestLowDuration = 0;
  //uint32 shortestLowDuration = -1; // == UMAX
  //uint32 longestHighDuration = 0;
  //uint32 shortestHighDuration = -1; // == UMAX

  int sign=1;
  long value=0;
  int i=0;
  // when we get here we know that we are at rising edge (HIGH)
  for (i=0;i<24;i++) {
    //samples[i]='?';


    //tempmicros = dialMicros();
    while (dialDigitalValue(CLOCK_IN_PIN)) { os_delay_us(1); } //wait until clock returns to LOW- the first bit is not needed
    //duration = dialMicros()-tempmicros;
    //if (duration>longestHighDuration) longestHighDuration = duration;
    //if (duration<shortestHighDuration) shortestHighDuration = duration;

    // we sample at clock falling edge
    if (dialDigitalValue(DATA_IN_PIN)) {
      //samples[i]='1';
      if (i<23) {
        value |= 1<<i;
      } else if (i==23) {
        value |= 0xff000000;
      }
    } else {
      //samples[i]='0';
    }

    //tempmicros = dialMicros();
    while (!dialDigitalValue(CLOCK_IN_PIN)) { os_delay_us(1);} //wait until clock returns to HIGH
    //duration = dialMicros()-tempmicros;
    //if (duration>longestLowDuration) longestLowDuration = duration;
    //if (duration<shortestLowDuration) shortestLowDuration = duration;



  }
  //samples[i]='\0';
  float fvalue = ((float)value)*1.2393573842291065e-04;
  //os_printf("dial: found>");
  //os_printf(samples);
  //os_printf("< lld=%d sld=%d lhd=%d shd=%d 100000*fvalue=%d value=%d\r\n", longestLowDuration, shortestLowDuration, longestHighDuration, shortestHighDuration, (int) fvalue, value);

  *returnValue= -fvalue;

  return true;
}

/**
 * Will set the input pin to inputs.
 */
void dialInit() {
  //os_printf("dial: Setting pins as input\r\n");

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

  GPIO_DIS_OUTPUT(CLOCK_IN_PIN);
  GPIO_DIS_OUTPUT(DATA_IN_PIN);
}

bool readDial(float *sample)
{
  uint32 i = 0;
  uint32 j = 0;
  uint32 duration = 0;
  uint32 tempmicros = 0;
  uint32 longestDuration = 0;

  // just to be safe - set pins as input once again
  dialInit();

  for (j = 0; j < 24 && duration < 500; j++){
    //if clock is HIGH wait until it turns to LOW
    for (i=0; dialDigitalValue(CLOCK_IN_PIN); i++) {
      os_delay_us(1);
      if(i==100000){
        os_printf("dial: No Data 1\r\n");
        return false;
      }
    }
    tempmicros = dialMicros();

    //wait for the end of the LOW pulse
    for (i=0; !dialDigitalValue(CLOCK_IN_PIN); i++ ) {
      os_delay_us(1);
      if(i==100000){
        os_printf("dial: No Data 2\r\n");
        return false;
      }
    }
    duration = dialMicros()-tempmicros;
    if (duration>longestDuration) longestDuration = duration;

  }

  if (j<24) { //if the LOW pulse was longer than 500 micros we are at the start of a new bit sequence
    // we are not at a rising edge
    dialDecode(sample); //dialDecode the bit sequence
    //os_printf("started scanning and found something %d us later ld=%d. Found %d\r\n" , duration, longestDuration, (int)(100* *sample));
    return true;
  } else {
    os_printf("dial: started scanning and gave up %d us later ld=%d.\r\n" , duration, longestDuration);
    return false;
  }
}

bool readDialAsString(char *buf, int bufLen, int *bytesWritten){
  float sample = 0.0;
  bool rv = readDial(&sample);
  if(rv){
    *bytesWritten = dro_utils_float_2_string(10000.0*sample, 10000, buf, bufLen);
  } else {
    *bytesWritten = 0;
    buf[0] = '\0';
  }
  return rv;
}

