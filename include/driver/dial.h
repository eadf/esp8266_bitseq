/*
 * dial.h
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */

#ifndef USER_dial_H_
#define USER_dial_H_

#include "c_types.h"
#include "os_type.h"

bool dial_read(float *sample);
bool dial_readAsString(char *sample, int bufLen, int *bytesWritten);
//bool dial_isIdle(void);
bool dial_startSampling(void);
void dial_init(os_timer_func_t *resultCb);

#endif /* USER_dial_H_ */
