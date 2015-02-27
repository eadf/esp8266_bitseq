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

/**
 * initiates the watt / power meter sampler
 * negativeLogic: set this to true if the signal is inverted
 * resultCb: pointer to the callback function
 * clockPin: the GPIO pin for the clock signal (can be any GPIO supported by easygpio)
 * dataPin: the GPIO pin for the data signal (can be any GPIO supported by easygpio)
 */
void watt_init(bool negativeLogic, os_timer_func_t *resultCb, uint8_t clockPin, uint8_t dataPin);

#endif /* USER_watt_H_ */
