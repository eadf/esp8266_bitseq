/*
 * gpio_intr.c
 *
 *  Generic bit GPIOI_sampler. GPIOI_samples the value of GPIOI_DATA_PIN every falling or rising edge of GPIOI_CLK_PIN.
 *
 *
 *  Created on: Jan 7, 2015
 *      Author: Ead Fritz
 */

#include "bitseq/gpio_intr.h"
#include "osapi.h"
#include "ets_sys.h"
#include "gpio.h"
#include "mem.h"

#define GPIOI_CLK_PIN 0   // only pin 0 is implemented
#define GPIOI_DATA_PIN 2  // pin 2 or 3 is implemented

// GPIOI_BUFFER_MASK must equal the bits needed to address GPIOI_BUFFER_SIZE
#define GPIOI_BUFFER_SIZE 512  //4096 bits
#define GPIOI_BUFFER_MASK 0x0FFF

static volatile os_timer_t callbackTimer;
volatile static GPIOI_Setting settings;
volatile static GPIOI_Result results;

static volatile uint32_t now;
static volatile uint32_t period;
static volatile uint32_t GPIOI_sample;

// forward declarations
void GPIOI_clearResults(void);
uint8_t bitAt(uint16_t bitNumber);
static void GPIOI_CLK_PIN_intr_handler(int8_t key);
void GPIOI_printBinary(uint32_t data, int untilBit);
void GPIOI_printBufferBinary(int16_t msb, int16_t lsb);

#define GPIOI_micros (0x7FFFFFFF & system_get_time())

void
GPIOI_disableInterrupt(void) {
  //disable interrupt
  gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_DISABLE);
  results.statusBits &= ~GPIOI_ISRUNNING;
}

void ICACHE_FLASH_ATTR
GPIOI_enableInterrupt(void) {
  if (! (results.statusBits & GPIOI_INITIATED) ) {
    os_printf("Error: GPIOI_enableInterrupt called before GPIOI_init\n");
  } else {
    GPIOI_clearResults();
    results.statusBits |= GPIOI_ISRUNNING;
    results.statusBits &= ~GPIOI_RESULT_IS_READY;
    results.lastTimestamp = GPIOI_micros;
    results.noOfRestarts = 0;
    //enable interrupt
    if (settings.onRising) {
      gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_POSEDGE);
    } else {
      gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_NEGEDGE);
    }
  }
}

void ICACHE_FLASH_ATTR
GPIOI_clearResults(void){
  results.currentBit = 0; // currentBit is truncated at GPIOI_BUFFER_SIZE
  results.sampledBits = 0; // GPIOI_sampledBits is truncated at uint16_t
  results.noOfRestarts = 0;
  results.statusBits &= GPIOI_INITIATED; // only keep initiated flag
  //results.statusBits &= ~(GPIOI_RESULT_IS_READY|GPIOI_INTERRUPT_WHILE_NOT_RUNNING|
  //                        GPIOI_INTERRUPT_WHILE_HAVE_RESULT|GPIOI_NOT_ENOUGH_SILENCE|
  //                        GPIOI_TOO_MUCH_SILENCE);
}

// quick and dirty implementation of sprintf with %f
int ICACHE_FLASH_ATTR
GPIOI_float_2_string(float sample, int divisor, char *buf, int bufLen) {
  char localBuffer[256];

  char *sign;
  if (sample>=0){
    sign = "";
  } else {
    sign = "-";
    sample = -sample;
  }

  int h = (int)(sample / divisor);
  int r = (int)(sample - h*divisor);
  int size = 0;

  switch (divisor){
    case 1: size = os_sprintf(localBuffer, "%s%d",sign,h);
      break;
    case 10: size = os_sprintf(localBuffer, "%s%d.%01d",sign,h, r);
      break;
    case 100: size = os_sprintf(localBuffer, "%s%d.%02d",sign,h, r);
      break;
    case 1000: size = os_sprintf(localBuffer, "%s%d.%03d",sign,h, r);
      break;
    case 10000: size = os_sprintf(localBuffer, "%s%d.%04d",sign,h, r);
      break;
    case 100000: size = os_sprintf(localBuffer, "%s%d.%05d",sign,h, r);
      break;
    default:
      os_printf("dro_utils_float_2_string: could not recognize divisor: %d\r\n", divisor);
     return 0;
  }
  int l = size>bufLen?bufLen:size;
  strncpy(buf,localBuffer,l);
  buf[l] = 0;
  return l;
}

void ICACHE_FLASH_ATTR
GPIOI_printBinary(uint32_t data, int untilBit) {
  int j;
  for ( j=untilBit; j>=0;j--){
    os_printf((data>>j)&1?"1":"0");
    if (j!=untilBit && (j%8)==0) {
      os_printf(" ");
    }
  }
}

void ICACHE_FLASH_ATTR
GPIOI_printBinary8(uint8_t data) {
  GPIOI_printBinary(data, 7);
}

void ICACHE_FLASH_ATTR
GPIOI_printBinary16(uint16_t data){
  GPIOI_printBinary(data, 15);
}

void ICACHE_FLASH_ATTR
GPIOI_printBinary32(uint32_t data){
  GPIOI_printBinary(data, 31);
}

/**
 * returns the bit value indicated by index
 */
uint8_t ICACHE_FLASH_ATTR
bitAt(uint16_t bitNumber) {
  bitNumber &= GPIOI_BUFFER_MASK;
  uint16_t bit  = bitNumber & 0x7;
  uint16_t byte = bitNumber >> 3;
  return (results.data[byte] >> (bit)) &1;
}

/**
 * slice out bits from the bit stream and print.
 * bit numbering are inclusive.
 *
 * If msb or lsb are negative they will count backward from
 * the last GPIOI_sampled bit +1.
 * i.e. -1 will indicate the last GPIOI_sampled bit.
 */
void ICACHE_FLASH_ATTR
GPIOI_printBufferBinary(int16_t msb, int16_t lsb) {
  if (msb<0) {
    msb = (results.currentBit+msb) & GPIOI_BUFFER_MASK;
  }
  if (lsb<0) {
    lsb = (results.currentBit+lsb) & GPIOI_BUFFER_MASK;
  }
  int bit = 0;
  int i = 0;
  if (msb < lsb){
    for (bit=msb; bit<=lsb; bit++, i++){
      os_printf(bitAt(bit)?"1":"0");
      if ((i+1)%8==0) {
        os_printf(" ");
      }
    }
  } else {
    for (bit=msb; bit>=lsb; bit--, i++){
      os_printf(bitAt(bit)?"1":"0");
      if ((i+1)%8==0) {
        os_printf(" ");
      }
    }
  }
}

/**
 * slice out integer values from the bit stream.
 * bit numbering are inclusive.
 *
 * If msb or lsb are negative they will count backward from
 * the last GPIOI_sampled bit +1.
 * i.e. -1 will indicate the last GPIOI_sampled bit.
 */
uint32_t ICACHE_FLASH_ATTR
GPIOI_sliceBits(int16_t msb, int16_t lsb, bool duplicateMsb){
  if (msb<0) {
    msb = results.currentBit+msb;
  }
  if (lsb<0) {
    lsb = results.currentBit+lsb;
  }
  uint32_t tmp = 0;
  int bit = 0;
  if (msb < lsb){
    if (duplicateMsb && bitAt(msb)) {
      tmp = 0xFFFFFFFF;
    }
    for (bit=msb; bit<=lsb; bit++){
      tmp = tmp << 1;
      tmp |= bitAt(bit);
    }
  } else {
    if (duplicateMsb && bitAt(msb)) {
      tmp = 0xFFFFFFFF;
    }
    for (bit=msb; bit>=lsb; bit--){
      tmp = tmp << 1;
      tmp |= bitAt(bit);
    }
  }
  return tmp;
}

/**
 * Store the bit value of 'GPIOI_sample' in 'results.data' at position 'results.currentBit'
 * increments results.currentBit.
 */
#define GPIOI_storeBit {                    \
  uint16_t aByte = results.currentBit >> 3; \
  uint16_t aBit = results.currentBit & 0x7; \
  if (GPIOI_sample) {                       \
    results.data[aByte] |= 1<<aBit;         \
  } else {                                  \
    results.data[aByte] &= ~(1 << aBit);    \
  }                                         \
  results.currentBit = (results.currentBit+1) & GPIOI_BUFFER_MASK; \
}

/**
 * This GPIOI_sampler will start sampling directly.
 * When it detects the idle period it tests if enough bits have been acquired.
 * It will continue/restart sampling if not enough bits have been stored.
 */
static void
GPIOI_CLK_PIN_intr_handler(int8_t key) {

  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  // clear interrupt status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));
  now = GPIOI_micros;
  GPIOI_sample = GPIO_INPUT_GET(GPIOI_DATA_PIN);
  period = now-results.lastTimestamp;

  if (GPIOI_RESULT_IS_READY & results.statusBits) {
    return; // Should not happen
  }

  if (period > settings.minIdlePeriod){
    if ( results.sampledBits >= settings.numberOfBits) {
      // we're done
      GPIOI_disableInterrupt();
      results.statusBits |= GPIOI_RESULT_IS_READY;
      os_timer_arm(&callbackTimer, 1, 0);
    } else {
      // start over
      results.currentBit = 0;
      results.noOfRestarts += 1;
      GPIOI_storeBit
      results.sampledBits += 1;
    }
  } else {
    GPIOI_storeBit
    results.sampledBits += 1;
  }
  results.lastTimestamp = now;
}

volatile GPIOI_Result volatile* ICACHE_FLASH_ATTR
GPIOI_getResult(void) {
  return &results;
}

bool ICACHE_FLASH_ATTR
GPIOI_isRunning(void) {
  bool rv = (results.statusBits & GPIOI_INITIATED) &&
            (results.statusBits & GPIOI_ISRUNNING);
  //os_printf("GPIOI: GPIOI_isRunning");GPIOI_printByteAsBinary(results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

bool ICACHE_FLASH_ATTR
GPIOI_isIdle(void) {
  bool rv = (results.statusBits & GPIOI_INITIATED) &&
            !(results.statusBits & GPIOI_ISRUNNING);
  //os_printf("GPIOI: GPIOI_isRunning");GPIOI_printByteAsBinary(results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

bool ICACHE_FLASH_ATTR
GPIOI_hasResults(void) {
  bool rv = (results.statusBits & GPIOI_INITIATED) &&
            (results.statusBits & GPIOI_RESULT_IS_READY);
  //os_printf("GPIOI: GPIOI_isRunning");GPIOI_printByteAsBinary(results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

void ICACHE_FLASH_ATTR
GPIOI_debugTrace(int16_t msb, int16_t lsb) {
  //os_printf("%02X%02X%02X%02X-",results.data[0],results.data[1],results.data[2],results.data[3]);
  GPIOI_printBufferBinary(msb, lsb);
  os_printf(" %06X restarts:%d sampledB:%d currB:%d sb:",
    GPIOI_sliceBits(msb, lsb, false),
    results.noOfRestarts, // restarts = noOfRestarts
    results.sampledBits,  // sampledB = sampledBits
    results.currentBit   // cb = current bit
    );
  GPIOI_printBinary16(results.statusBits);
  os_printf("\n\r");
}

/**
 * numberOfBits: the number of bits we are going to sample
 * minIdlePeriod: at least this many us between blocks
 * onRising : rising or falling edge trigger
 * resultCb : the callback to call when results are available
 */
void ICACHE_FLASH_ATTR
GPIOI_init(uint16_t numberOfBits, uint32_t minIdlePeriod, bool onRising, os_timer_func_t *resultCb) {

  //Setup callback timer
  os_timer_disarm(&callbackTimer);
  os_timer_setfn(&callbackTimer, resultCb, NULL);

  settings.minIdlePeriod = minIdlePeriod;
  settings.numberOfBits = numberOfBits;
  settings.onRising = onRising;

  results.data = (uint8_t*)os_malloc(GPIOI_BUFFER_SIZE);
  results.statusBits = 0;

  GPIOI_clearResults();

  if (0 == GPIOI_CLK_PIN){
    //set gpio0 as gpio pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    //disable pull downs
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO0_U);
    //disable pull ups
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);
    // disable output
    GPIO_DIS_OUTPUT(GPIOI_CLK_PIN);
    ETS_GPIO_INTR_ATTACH(GPIOI_CLK_PIN_intr_handler,0);
    ETS_GPIO_INTR_DISABLE();
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(GPIOI_CLK_PIN));
    gpio_register_set(GPIO_PIN_ADDR(0), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
        | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
        | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
  } else {
    os_printf("GPIOI_init: Error GPIOI_CLK_PIN==%d is not implemented", GPIOI_CLK_PIN);
    return;
  }

  if (2 == GPIOI_DATA_PIN){
    //set gpio2 as gpio pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    //disable pull downs
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO2_U);
    //disable pull ups
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
    // disable output
    GPIO_DIS_OUTPUT(GPIOI_DATA_PIN);
  } else if (3 == GPIOI_DATA_PIN) {
    //set gpio2 as gpio pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
    //disable pull downs
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_U0RXD_U);
    //disable pull ups
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
    // disable output
    GPIO_DIS_OUTPUT(GPIOI_DATA_PIN);
  } else {
    os_printf("GPIOI_init: Error GPIOI_DATA_PIN==%d is not implemented", GPIOI_DATA_PIN);
    return;
  }
  os_printf("GPIOI_init: Initiated the GPIOI GPIOI_sampler with GPIOI_CLK_PIN=%d and GPIOI_DATA_PIN=%d.", GPIOI_CLK_PIN, GPIOI_DATA_PIN);
  os_printf("GPIOI_init: Will sample %d bits on the %s edge", settings.numberOfBits, settings.onRising?"rising":"falling");

  //clear gpio status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));
  GPIOI_disableInterrupt();
  ETS_GPIO_INTR_ENABLE();

  results.statusBits |= GPIOI_INITIATED;
}

