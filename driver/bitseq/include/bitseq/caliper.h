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

/**
 * initiates the caliper sampler
 * negativeLogic: set this to true if the signal is inverted
 * resultCb: pointer to the callback function
 * clockPin: the GPIO pin for the clock signal (can be any GPIO supported by easygpio)
 * dataPin: the GPIO pin for the data signal (can be any GPIO supported by easygpio)
 */
void caliper_init(bool negativeLogic, os_timer_func_t *resultCb, uint8_t clockPin, uint8_t dataPin);

#endif /* USER_CALIPER_H_ */
