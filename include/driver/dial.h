/*
 * dial.h
 *
 *  Created on: Jan 2, 2015
 *      Author: user
 */

#ifndef USER_dial_H_
#define USER_dial_H_

#include "c_types.h"

void dialInit();
bool readDial(float *sample);
bool readDialAsString(char *sample, int bufLen, int *bytesWritten);

#endif /* USER_dial_H_ */
