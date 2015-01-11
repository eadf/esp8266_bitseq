/*
 * caliper.h
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */

#ifndef USER_CALIPER_H_
#define USER_CALIPER_H_

#include "c_types.h" // for bool
#include "os_type.h"

bool readCaliper(float *sample, bool *isMM);
bool readCaliperAsString(char *sample, int bufLen, int *bytesWritten);
bool startCaliperSample(void);
void caliperInit(os_timer_func_t *resultCb);

#endif /* USER_CALIPER_H_ */
