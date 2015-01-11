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

bool readDial(float *sample);
bool readDialAsString(char *sample, int bufLen, int *bytesWritten);
bool isDialIdle(void);
bool startDialSample(void);
void dialInit(os_timer_func_t *resultCb);

#endif /* USER_dial_H_ */
