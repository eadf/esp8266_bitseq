/*
 * watt.h
 *
 *  Created on: Jan 2, 2015
 *      Author: eadf
 */

#ifndef USER_watt_H_
#define USER_watt_H_

#include "c_types.h"
#include "os_type.h"

bool watt_read(float *sample);
bool watt_readAsString(char *sample, int bufLen, int *bytesWritten);
bool watt_startSampling(void);
void watt_init(bool negativeLogic, os_timer_func_t *resultCb);

#endif /* USER_watt_H_ */
