/*
 * gpio_intr.c
 *
 *  Generic bit sampler. Samples the value of GPIOI_DATA_PIN every falling or rising edge of GPIOI_CLK_PIN.
 *
 *
 *  Created on: Jan 7, 2015
 *      Author: Ead Fritz
 */

#include "driver/gpio_intr.h"
#include "osapi.h"
#include "ets_sys.h"
#include "os_type.h"
#include "gpio.h"
#include "mem.h"

static const uint16_t GPIOI_CLK_PIN = 0;
static const uint16_t GPIOI_DATA_PIN = 2;
//static uint8_t sampleBuffer[1024];

volatile static GPIOI_Setting settings;
volatile static GPIOI_Result results;

void GPIOI_clearResults(void);
void GPIOI_clearSampleData(void);
uint32_t GPIOI_sliceBits(uint16_t start, uint16_t end);
void GPIOI_inputSampleBit(uint8_t sample);

void ICACHE_FLASH_ATTR
GPIOI_disableInterrupt(void) {
  //disable interrupt
  gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_DISABLE);
}

void ICACHE_FLASH_ATTR
GPIOI_enableInterrupt(void) {
  if (! (results.statusBits & GPIOI_INITIATED) ) {
    os_printf("Error: GPIOI_enableInterrupt called before GPIOI_init\n");
  } else {
    GPIOI_clearResults();
    results.statusBits |= GPIOI_ISRUNNING;
    results.statusBits &= ~GPIOI_RESULT_IS_READY;
    results.lastTimestamp = GPIOI_micros();

    //enable interrupt
    if (settings.onRising) {
      gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_POSEDGE);
    } else {
      gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_NEGEDGE);
    }
  }
}

uint32_t ICACHE_FLASH_ATTR
GPIOI_micros(void) {
  return 0x7FFFFFFF & system_get_time();
}

void ICACHE_FLASH_ATTR
GPIOI_clearSampleData(void) {
  int byte = (settings.numberOfBits-1) >> 3; // byte indexes
  for (; byte>=0; byte--) {
    (results.data)[byte] = 0;
  }
}

void ICACHE_FLASH_ATTR
GPIOI_clearResults(void){
  results.currentBit = 0;
  results.fastestPeriod = 0xFFFFFFFF;
  results.slowestPeriod = 0;
  results.bitZeroPeriod = 0;
  results.statusBits &= ~(GPIOI_RESULT_IS_READY|GPIOI_INTERRUPT_WHILE_NOT_RUNNING|
                          GPIOI_INTERRUPT_WHILE_HAVE_RESULT|GPIOI_NOT_ENOUGH_SILENCE|
                          GPIOI_TOO_MUCH_SILENCE|GPIOI_TOO_SLOW_CLOCK);
  GPIOI_clearSampleData();
}


void ICACHE_FLASH_ATTR
GPIOI_printByteAsBinary(uint8_t byte){
  int16_t bit;
  for (bit = 7; bit>=0; bit--) {
    os_printf("%d", byte >> bit & 1);
  }
}

void ICACHE_FLASH_ATTR
GPIOI_printBinary(void){
  int16_t byte = (settings.numberOfBits-1) >> 3;
  int16_t bit = (settings.numberOfBits-1) & 0x7;
  if (bit<8) {
    for (;bit>=0; bit--) {
      os_printf("%d", (results.data)[byte] >> bit &  1);
    }
    byte--;
    if (byte >=0) {
      os_printf(" ");
    }
  }
  for (; byte >=0; byte--){
    for (bit = 7; bit >=0; bit--){
      os_printf("%d", (results.data)[byte] >> bit &  1);
    }
    if (byte-1>=0){
      os_printf(" ");
    }
  }
}

/**
 * slice out integer values from the bit stream
 * bit numbering begins with 0
 */
uint32_t ICACHE_FLASH_ATTR
GPIOI_sliceBits(uint16_t start, uint16_t end){

  if (7==end-start && start%8==0) {
    // aligned int8_t slices
    int index = start/8;
    return (results.data)[index];
  } else if (15==end-start && start%16==0) {
    // aligned int16_t slices
    int index = start/16;
    uint32_t tmp = (results.data)[index+1];
    tmp <<= 8;
    tmp |= (results.data)[index];
    return tmp;
  } else if (31==end-start && start%32==0) {
    // aligned int32_t slices
    int index = start/32;
    uint32_t tmp = (results.data)[index+3];
    tmp <<= 8;
    tmp |= (results.data)[index+2];
    tmp <<= 8;
    tmp |= (results.data)[index+1];
    tmp <<= 8;
    tmp |= (results.data)[index];
    return tmp;
  } else {
    // non byte aligned slice - slow and inefficient
    int16_t bit;
    uint32_t tmp = 0;
    for (bit=end; bit>=start; bit--){
      tmp |= ((results.data)[bit>>3] >> (bit & 0x7)) & 1;
      if ( bit-1 >= start){
        tmp= tmp << 1;
      }
    }
    return tmp;
  }
}

void
GPIOI_inputSampleBit(uint8_t sample) {
  // sample must be 1 or 0
  sample &= 1;

#ifdef DRIVER_GPIO_INTR_ENABLE_HEARTBEAT
  results.heartbeat +=1;
#endif

  if (results.statusBits & GPIOI_RESULT_IS_READY) {
    results.statusBits |= GPIOI_INTERRUPT_WHILE_HAVE_RESULT;
    return; // we are already done here
  }
  if ( !(results.statusBits & GPIOI_ISRUNNING) ) {
    results.statusBits |= GPIOI_INTERRUPT_WHILE_NOT_RUNNING;
    return; // we are already done here
  }

  uint32_t now = GPIOI_micros();
  uint32_t period = now-results.lastTimestamp;

  if (results.currentBit==0){
    if (period < settings.minStartPeriod) {
      // There were not enough silence before this bit, start over
      results.statusBits |= GPIOI_NOT_ENOUGH_SILENCE;
      results.lastTimestamp = now;
      GPIOI_clearSampleData();
      return;
    }
    if ( period > settings.maxStartPeriod){
      // There were too much silence before this bit, start over
      results.statusBits |= GPIOI_TOO_MUCH_SILENCE;
      results.lastTimestamp = now;
      GPIOI_clearSampleData();
      return;
    }
    results.fastestPeriod = 0xFFFFFFFF;
    results.slowestPeriod = 0;
    results.bitZeroPeriod = period;
  } else {
    if (period > settings.maxClockPeriod){
      results.statusBits |= GPIOI_TOO_SLOW_CLOCK;
      // reset timing and start over
      results.currentBit = 0;
    } else {
      if (period < results.fastestPeriod) {
        results.fastestPeriod = period;
      }
      if (period > results.slowestPeriod){
        results.slowestPeriod = period;
      }
    }
  }
  results.lastTimestamp = now;

  int16_t byte = results.currentBit >> 3;
  uint8_t bit = results.currentBit & 0x7;
  (results.data)[byte] |= sample<<bit;

  results.currentBit += 1;

  if (results.currentBit >= settings.numberOfBits){
    GPIOI_disableInterrupt();
    results.statusBits |= GPIOI_RESULT_IS_READY;
    results.statusBits &= ~GPIOI_ISRUNNING;
  }
}

LOCAL void
GPIOI_CLK_PIN_intr_handler(int8_t key) {

  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  // clear interrupt status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));

  GPIOI_inputSampleBit(GPIO_INPUT_GET(GPIOI_DATA_PIN));
}


volatile GPIOI_Result* ICACHE_FLASH_ATTR
GPIOI_getResult(void) {
  return &results;
}

bool GPIOI_isRunning(void) {
  bool rv = (results.statusBits & GPIOI_INITIATED) &&
            (results.statusBits & GPIOI_ISRUNNING);
  //os_printf("GPIOI: GPIOI_isRunning");GPIOI_printByteAsBinary(results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

bool
GPIOI_hasResults(void) {
  bool rv = (results.statusBits & GPIOI_INITIATED) &&
            (results.statusBits & GPIOI_RESULT_IS_READY);
  //os_printf("GPIOI: GPIOI_isRunning");GPIOI_printByteAsBinary(results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

void ICACHE_FLASH_ATTR
GPIOI_debugTrace(uint16_t startBit, uint16_t endBit) {
  GPIOI_printBinary();

#ifdef DRIVER_GPIO_INTR_ENABLE_HEARTBEAT
  if (!results.statusBits & GPIOI_ISRUNNING ) {
    os_printf("GPIOI: Interrupt handler was never started.");
  }
  os_printf(" %06X fast:%d slow:%d zw:%d hb:%d currB:%d sb:",
        GPIOI_sliceBits(startBit, endBit),
        results.fastestPeriod,
        results.slowestPeriod,
        results.bitZeroPeriod,
        results.heartbeat,
        results.currentBit);
#else
   os_printf(" %06X fast:%d slow:%d zw:%d currB:%d sb:",
      GPIOI_sliceBits(startBit, endBit),
      results.fastestPeriod,
      results.slowestPeriod,
      results.bitZeroPeriod,
      results.currentBit );
#endif
   GPIOI_printByteAsBinary(results.statusBits);
   os_printf("\n\r");
}

/**
 * numberOfBits: the number of bits we are going to sample
 * maxClockPeriod: the maximum length of a clock period
 * minStartPeriod: at least this many us between blocks
 * maxStartPeriod: at most this many us between blocks
 */
void
GPIOI_init(uint16_t numberOfBits, uint32_t maxClockPeriod, uint32_t minStartPeriod, uint32_t maxStartPeriod, bool onRising) {

  settings.maxClockPeriod = maxClockPeriod;
  settings.minStartPeriod = minStartPeriod;
  settings.maxStartPeriod = maxStartPeriod;
  settings.numberOfBits = numberOfBits;
  settings.onRising = onRising;

  int size = (numberOfBits>>3) +2;
  os_printf("****\n");
  os_printf("GPIOI_init: Allocating %d bytes for receiver buffer.\n", size);
  os_printf("****\n");

  results.data = (uint8_t*)os_malloc(512);
  results.statusBits = 0;

  os_printf("****\n");
  os_printf("GPIOI_init: Allocated %d bytes for receiver buffer.\n", size);
  os_printf("****\n");

  GPIOI_clearResults();

  //set gpio0 as gpio pin
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
  //set gpio2 as gpio pin
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

  //disable pull downs
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO2_U);
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO0_U);

  //disable pull ups
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);

  // disable output
  GPIO_DIS_OUTPUT(GPIOI_CLK_PIN);
  GPIO_DIS_OUTPUT(GPIOI_DATA_PIN);

  ETS_GPIO_INTR_ATTACH(GPIOI_CLK_PIN_intr_handler,0);
  ETS_GPIO_INTR_DISABLE();
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
  gpio_output_set(0, 0, 0, GPIO_ID_PIN(GPIOI_CLK_PIN));
  gpio_register_set(GPIO_PIN_ADDR(0), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
      | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
      | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));

  //clear gpio status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));

  GPIOI_disableInterrupt();

  ETS_GPIO_INTR_ENABLE();

  results.statusBits |= GPIOI_INITIATED;
}

