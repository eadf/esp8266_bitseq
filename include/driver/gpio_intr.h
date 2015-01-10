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

typedef enum {GPIOI_INTERRUPT_WHILE_NOT_RUNNING=1<<0, // we are not supposed to get these
              GPIOI_INTERRUPT_WHILE_HAVE_RESULT=1<<1, // we are not supposed to get these
              GPIOI_NOT_ENOUGH_SILENCE=1<<2,
              GPIOI_TOO_MUCH_SILENCE=1<<3,
              GPIOI_TOO_SLOW_CLOCK=1<<4,
              GPIOI_INITIATED=1<<5,
              GPIOI_ISRUNNING=1<<6,
              GPIOI_RESULT_IS_READY=1<7} GPIOI_statusbits;

typedef struct {
  uint16_t numberOfBits;    // the number of bits we are going to sample
  uint32_t maxClockPeriod;  // the maximum length of a clock period
  uint32_t minStartPeriod;  // at least this many us between blocks
  uint32_t maxStartPeriod;  // at most this many us between blocks
  bool onRising;            // what clock pulse edge to sample on
} GPIOI_Setting;

typedef struct {
  volatile uint32_t fastestPeriod;
  volatile uint32_t slowestPeriod;
  volatile uint32_t bitZeroPeriod;

#ifdef DRIVER_GPIO_INTR_ENABLE_HEARTBEAT
  volatile uint32_t heartbeat;
#endif

  volatile uint32_t lastTimestamp;
  volatile uint32_t statusBits;
  volatile uint16_t currentBit;
  volatile uint16_t filler1;
  volatile uint8_t  *data;
} GPIOI_Result;

void GPIOI_init(uint16_t numberOfBits, uint32_t maxClockPeriod, uint32_t minStartPeriod, uint32_t maxStartPeriod, bool onRising);
bool GPIOI_hasResults(void);
bool GPIOI_isRunning(void);
void GPIOI_enableInterrupt(void);
void GPIOI_disableInterrupt(void);
uint32_t GPIOI_micros(void);

/**
 * returns a pointer to the result structure, or NULL if there are no results.
 */
volatile GPIOI_Result* GPIOI_getResult(void);
void GPIOI_printBinary(void);
void GPIOI_printByteAsBinary(uint8_t byte);
uint32_t GPIOI_sliceBits(uint16_t startBit, uint16_t endBit);
void GPIOI_debugTrace(uint16_t startBit, uint16_t endBit);

#endif /* INCLUDE_DRIVER_GPIO_INTR_H_ */
