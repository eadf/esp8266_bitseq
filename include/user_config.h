#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define SENSOR_SAMPLE_PERIOD 1000 // in milli-seconds

// Only one of USE_WATT_SENSOR, USE_DIAL_SENSOR or USE_CALIPER_SENSOR can be active at a time
//#define USE_DIAL_SENSOR
//#define USE_CALIPER_SENSOR
#define USE_WATT_SENSOR

// The GPIO pin your are using as clock
#define BITSEQ_CLOCK_PIN 0
// The GPIO pin your are using as data
#define BITSEQ_DATA_PIN 2
// set this to true if your input is inverted
#define BITSEQ_NEGATIVE_LOGIC false
// if this is defined a debug trace of the raw input bits will be printed
//#define BITSEQ_DEBUG_RAW
#endif
