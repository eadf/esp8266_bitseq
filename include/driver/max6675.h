/*
 * max6675.h
 *
 *  This is a direct port of the Arduino library found at https://github.com/adafruit/MAX6675-library
 *
 * this library is public domain. enjoy!
 * www.ladyada.net/learn/sensors/thermocouple
 */

#ifndef INCLUDE_DRIVER_MAX6675_H_
#define INCLUDE_DRIVER_MAX6675_H_

#include "c_types.h"
void max6675_init(void);

bool max6675_readTemp(float* sample, bool celcius);

/**
 * reads the temperature and fills the buf with the value as a string.
 */
bool max6675_readTempAsString(char *buf, int bufLen, int *bytesWritten, bool celcius);

#endif /* INCLUDE_DRIVER_MAX6675_H_ */
