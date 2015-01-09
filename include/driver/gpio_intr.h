/*
 * gpio_intr.h
 *
 *  Created on: Jan 7, 2015
 *      Author: user
 */

#ifndef INCLUDE_DRIVER_GPIO_INTR_H_
#define INCLUDE_DRIVER_GPIO_INTR_H_

#include "c_types.h"

#define DRIVER_GPIO_INTR_ENABLE_HEARTBEAT

/**
 * numberOfBits: the number of bits we are going to sample
 * maxClockPeriod: the maximum length of a clock period
 * minStartPeriod: at least this many us between blocks
 * maxStartPeriod: at most this many us between blocks
 */
void GPIOI_init(uint16_t numberOfBits, uint32_t maxClockPeriod, uint32_t minStartPeriod, uint32_t maxStartPeriod, bool onRising);
bool GPIOI_isRunning(void);
void GPIOI_enableInterrupt(void);
void GPIOI_disableInterrupt(void);
uint32_t GPIOI_micros(void);
uint32_t GPIOI_getHeartbeat(void);

uint32_t GPIOI_getResult(uint32_t *fastestPeriod, uint32_t *slowestPeriod, uint32_t *bitZeroPeriod, uint8_t *statusBits, uint16_t *currentBit);

void GPIOIprintBinary(uint32_t data);

typedef enum {GPIOI_ALREADY_DONE=1, GPIOI_NOT_ENOUGH_SILENCE=2, GPIOI_TOO_MUCH_SILENCE=4} GPIOI_statusbits;

#endif /* INCLUDE_DRIVER_GPIO_INTR_H_ */
