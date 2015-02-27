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
bool dial_startSampling(void);

/**
 * initiates the dial sampler
 * negativeLogic: set this to true if the signal is inverted
 * resultCb: pointer to the callback function
 * clockPin: the GPIO pin for the clock signal (can be any GPIO supported by easygpio)
 * dataPin: the GPIO pin for the data signal (can be any GPIO supported by easygpio)
 */
void dial_init(bool negativeLogic, os_timer_func_t *resultCb, uint8_t clockPin, uint8_t dataPin);

#endif /* USER_dial_H_ */
