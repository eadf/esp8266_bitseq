/*
 * gpio_intr.h
 *
 *  Created on: Jan 7, 2015
 *      Author: eadf
 */

#ifndef INCLUDE_DRIVER_GPIO_INTR_H_
#define INCLUDE_DRIVER_GPIO_INTR_H_

#include "c_types.h"
#include "os_type.h"

#undef DRIVER_GPIO_INTR_ENABLE_SAMPLING_DEBUG
#define DRIVER_GPIO_INTR_ENABLE_SAMPLING_DEBUG

typedef enum {GPIOI_INTERRUPT_WHILE_HAVE_RESULT=1<<1, // we are not supposed to get these
              GPIOI_NOT_ENOUGH_SILENCE=1<<2,
              GPIOI_TOO_MUCH_SILENCE=1<<3,
              GPIOI_INTERRUPT_WHILE_NOT_RUNNING=1<<4,
              GPIOI_INITIATED=1<<5,
              GPIOI_ISRUNNING=1<<6,
              GPIOI_RESULT_IS_READY=1<7,
              GPIOI_LAST_INTERRUPT_NOT_FINISHED=1<8} GPIOI_statusbits;

typedef struct {
  uint16_t numberOfBits;    // the number of bits we are going to sample
  uint32_t minIdlePeriod;  // at least this many us between blocks
  bool onRising;            // what clock pulse edge to sample on
} GPIOI_Setting;

typedef struct {

  volatile uint32_t lastTimestamp;
  volatile uint32_t noOfRestarts;
  volatile uint32_t statusBits;
  volatile uint16_t currentBit;
  volatile uint16_t sampledBits;
  volatile uint16_t filler1;
  volatile uint8_t  volatile * volatile data;
} GPIOI_Result;

void GPIOI_init(uint16_t numberOfBits, uint32_t minIdlePeriod, bool onRising, os_timer_func_t *resultCb);
bool GPIOI_hasResults(void);
bool GPIOI_isIdle(void);
bool GPIOI_isRunning(void);
void GPIOI_enableInterrupt(void);
void GPIOI_disableInterrupt(void);

/**
 * returns a pointer to the result structure, or NULL if there are no results.
 */
volatile GPIOI_Result* GPIOI_getResult(void);

int GPIOI_float_2_string(float sample, int divisor, char *buf, int bufLen);
void GPIOI_printBinary8(uint8_t data);
void GPIOI_printBinary16(uint16_t data);
void GPIOI_printBinary32(uint32_t data);

/**
 * slice out integer values from the bit stream.
 * bit numbering are inclusive.
 *
 * If msb or lsb are negative they will count backward from
 * the last sampled bit +1.
 * i.e. -1 will indicate the last sampled bit.
 */
uint32_t GPIOI_sliceBits(int16_t msb, int16_t lsb, bool duplicateMsb);
void GPIOI_debugTrace(int16_t msb, int16_t lsb);

#endif /* INCLUDE_DRIVER_GPIO_INTR_H_ */
