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

bool caliper_read(float *sample, bool *isMM);
bool caliper_readAsString(char *sample, int bufLen, int *bytesWritten);
bool caliper_startSampling(void);
void caliper_init(os_timer_func_t *resultCb);

#endif /* USER_CALIPER_H_ */
