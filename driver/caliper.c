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

static const int CLOCK_IN_PIN = 0;
static const int DATA_IN_PIN = 2;
static const float CONVERT_TO_MM = 0.01f;
static bool initIsCalled = false;

/* forward declarations to shut up the compiler -Werror=missing-prototypes */
bool caliperDigitalValue(int pin);
bool caliperDecode_BITBANG(float *returnValue);
bool readCaliper_BITBANG(float *sample);

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
    while (caliperDigitalValue(CLOCK_IN_PIN)) { os_delay_us(1); } //wait until clock returns to HIGH- the first bit is not needed
    while (!caliperDigitalValue(CLOCK_IN_PIN)) { os_delay_us(1);} //wait until clock returns to LOW
    if (!caliperDigitalValue(DATA_IN_PIN)) {
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
void ICACHE_FLASH_ATTR
caliperInit(void) {
  // Acquire 24 pins
  // at most 900 us between clock pulses
  // at least 10 ms between blocks
  // at most 5 sec between blocks
  // rising edge
  GPIOI_init(24, 900, 10000, 5000000, true);
  //GPIOI_init(24, 40000, 1, 5000000, true);
  initIsCalled = true;
}

bool ICACHE_FLASH_ATTR
readCaliper_BITBANG(float *sample) {
  uint32 i = 0;

  // just to be safe - set pins as input once again
  //caliperInit();

  //if clock is LOW wait until it turns to HIGH
  for (i=0; caliperDigitalValue(CLOCK_IN_PIN); ) {
    os_delay_us(1);
    i++;
    if(i==100000){
      //os_printf("Caliper: No Data 1\r\n");
      return false;
    }
  }
  uint32 tempmicros = dro_utils_micros();

  //wait for the end of the HIGH pulse
  for (i=0; !caliperDigitalValue(CLOCK_IN_PIN); ) {
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


bool ICACHE_FLASH_ATTR
readCaliper(float *sample) {

  if (!initIsCalled) {
    os_printf("Error readCaliper: call caliperInit first!\n\r");
    return false;
  }

  uint16_t i = 0;
  uint32_t result;
  int32_t polishedResult;

  if (!GPIOI_isRunning()){
    os_printf("Setting new interrupt handler\n\r");
    GPIOI_enableInterrupt();
  }

  for (i=0; i<100; i++) {
    os_delay_us(20000); // 20ms

    if ( GPIOI_hasResults() ) {
      result = GPIOI_sliceBits(0,23);
      polishedResult = result;
      if (polishedResult & 1<<20 ) {
        // bit 20 signals negative value
        polishedResult &= 0xEFFFFF; // lower bit 20 again
        polishedResult = -polishedResult; // set it to negative value
      }

      os_printf("Result is %d : ", polishedResult);
      GPIOI_debugTrace(0,23);
      *sample = CONVERT_TO_MM * polishedResult;
      return true;
    }
  }

  os_printf("GPIOI Still running, tmp result is:\n");
  GPIOI_debugTrace(0,23);
  return false;
}


bool ICACHE_FLASH_ATTR
readCaliperAsString(char *buf, int bufLen, int *bytesWritten){
  float sample = 0.0;
  bool rv = readCaliper(&sample);
  if(rv){
    *bytesWritten = dro_utils_float_2_string(100.0f*sample, 100, buf, bufLen);
  } else {
    *bytesWritten = 0;
    buf[0] = 0;
  }
  return rv;
}
