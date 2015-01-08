/*
 * gpio_intr.h
 *
 *  Created on: Jan 7, 2015
 *      Author: user
 */

#ifndef INCLUDE_DRIVER_GPIO_INTR_H_
#define INCLUDE_DRIVER_GPIO_INTR_H_

#include "c_types.h"

/**
 * numberOfBits: the number of bits we are going to sample
 * maxClockPeriod: the maximum length of a clock period
 * minStartPeriod: at least this many us between blocks
 * maxStartPeriod: at most this many us between blocks
 */
void GPIOI_init(uint16_t numberOfBits, uint32_t maxClockPeriod, uint32_t minStartPeriod, uint32_t maxStartPeriod, bool onRising);
bool GPIOI_isRunning();
void GPIOI_enableInterrupt();
void GPIOI_disableInterrupt();
uint32_t GPIOI_getResult(uint32_t *fastestPeriod, uint32_t *slowestPeriod, uint32_t *bitZeroWait, uint16_t *currentBit);

void GPIOIprintBinary(uint32_t data);

#endif /* INCLUDE_DRIVER_GPIO_INTR_H_ */
