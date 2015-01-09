/*
 * caliper.h
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */

#ifndef USER_CALIPER_H_
#define USER_CALIPER_H_

#include "c_types.h" // for bool

bool readCaliper(float *sample);
bool readCaliperAsString(char *sample, int bufLen, int *bytesWritten);
void caliperInit(void);

#endif /* USER_CALIPER_H_ */
