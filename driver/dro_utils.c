/*
 * dro_utils.c
 *
 *  Created on: Jan 4, 2015
 *      Author: ead_fritz
 */

#include "osapi.h"
#include "driver/dro_utils.h"

// quick and dirty implementation of sprintf with %f
int ICACHE_FLASH_ATTR
dro_utils_float_2_string(float sample, int divisor, char *buf, int bufLen) {
  char localBuffer[256];

  int s = sample>0?1:-1;
  sample = sample*s;
  int h = (int)(sample / divisor);
  int r = (int)(sample - h*divisor);
  int size = 0;

  switch (divisor){
    case 1: size = os_sprintf(localBuffer, "%d",h);
      break;
    case 10: size = os_sprintf(localBuffer, "%d.%01d", s*h, r);
      break;
    case 100: size = os_sprintf(localBuffer, "%d.%02d", s*h, r);
      break;
    case 1000: size = os_sprintf(localBuffer, "%d.%03d", s*h, r);
      break;
    case 10000: size = os_sprintf(localBuffer, "%d.%04d", s*h, r);
      break;
    case 100000: size = os_sprintf(localBuffer, "%d.%05d", s*h, r);
      break;
    default:
      os_printf("dro_utils_float_2_string: could not recognize divisor: %d\r\n", divisor);
     return 0;
  }
  int l = size>bufLen?bufLen:size;
  strncpy(buf,localBuffer,l);
  buf[l] = 0;
  return l;
}

/**
 * caliperMicros() : return system timer in us
 */
uint32 ICACHE_FLASH_ATTR
dro_utils_micros(void) {
  return 0x7FFFFFFF & system_get_time();
}
