/*
 * bitseq.h
 *
 *  Created on: Jan 7, 2015
 *      Author: eadf
 */

#ifndef INCLUDE_BITSEQ_H_
#define INCLUDE_BITSEQ_H_

#include "c_types.h"
#include "os_type.h"

typedef enum {BITSEQ_INTERRUPT_WHILE_HAVE_RESULT=1<<1, // we are not supposed to get these
              BITSEQ_NOT_ENOUGH_SILENCE=1<<2,
              BITSEQ_TOO_MUCH_SILENCE=1<<3,
              BITSEQ_INTERRUPT_WHILE_NOT_RUNNING=1<<4,
              BITSEQ_INITIATED=1<<5,
              BITSEQ_ISRUNNING=1<<6,
              BITSEQ_RESULT_IS_READY=1<7,
              BITSEQ_LAST_INTERRUPT_NOT_FINISHED=1<8} bitseq_statusbits;

typedef struct {
  uint16_t numberOfBits;    // the number of bits we are going to sample
  uint32_t minIdlePeriod;  // at least this many us between blocks
  bool onRising;            // what clock pulse edge to sample on
} BitseqSetting;

typedef struct {

  volatile uint32_t lastTimestamp;
  volatile uint32_t noOfRestarts;
  volatile uint32_t statusBits;
  volatile uint16_t currentBit;
  volatile uint16_t sampledBits;
  volatile uint16_t filler1;
  volatile uint8_t  volatile * volatile data;
} BitseqResult;

void bitseq_init(uint16_t numberOfBits, uint32_t minIdlePeriod, bool onRising, os_timer_func_t *resultCb);
bool bitseq_hasResults(void);
bool bitseq_isIdle(void);
bool bitseq_isRunning(void);
void bitseq_enableInterrupt(void);
void bitseq_disableInterrupt(void);

/**
 * returns a pointer to the result structure, or NULL if there are no results.
 */
volatile BitseqResult* bitseq_getResult(void);

int bitseq_float2string(float sample, int divisor, char *buf, int bufLen);
void bitseq_printBinary8(uint8_t data);
void bitseq_printBinary16(uint16_t data);
void bitseq_printBinary32(uint32_t data);

/**
 * slice out integer values from the bit stream.
 * bit numbering are inclusive.
 *
 * If msb or lsb are negative they will count backward from
 * the last sampled bit +1.
 * i.e. -1 will indicate the last sampled bit.
 */
uint32_t bitseq_sliceBits(int16_t msb, int16_t lsb, bool duplicateMsb);
void bitseq_debugTrace(int16_t msb, int16_t lsb);

#endif /* INCLUDE_BITSEQ_H_ */
