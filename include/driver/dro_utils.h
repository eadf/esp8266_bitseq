/*
 * dro_utils.h
 *
 *  Created on: Jan 4, 2015
 *      Author: ead_fritz
 */

#ifndef USER_DRO_UTILS_H_
#define USER_DRO_UTILS_H_


/**
 * normal os_sprintf is broken. Instead of returning
 * a string:ified float it just returns a string containing "%f".
 * This is a quick and dirty hack to sidetrack the problem.
 *
 * Returns the number of chars written.
 */
int dro_utils_float_2_string(float sample, int divisor, char *buf, int bufLen);

uint32 dro_utils_micros(void);


#endif /* USER_DRO_UTILS_H_ */
