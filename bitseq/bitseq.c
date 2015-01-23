/*
 * gpio_intr.c
 *
 *  Generic bit sampler. Retreives the value of BITSEQ_DATA_PIN every falling or rising edge of BITSEQ_CLK_PIN.
 *
 *
 *  Created on: Jan 7, 2015
 *      Author: Ead Fritz
 */

#include "bitseq/bitseq.h"
#include "osapi.h"
#include "ets_sys.h"
#include "gpio.h"
#include "mem.h"

#define BITSEQ_CLK_PIN 0   // only pin 0 is implemented
#define BITSEQ_DATA_PIN 2  // pin 2 or 3 is implemented

// BITSEQ_BUFFER_MASK must equal the bits needed to address BITSEQ_BUFFER_SIZE
#define BITSEQ_BUFFER_SIZE 512  //4096 bits
#define BITSEQ_BUFFER_MASK 0x0FFF

static volatile os_timer_t bitseq_callbackTimer;
volatile static BitseqSetting bitseq_settings;
volatile static BitseqResult bitseq_results;

static volatile uint32_t bitseq_now;
static volatile uint32_t bitseq_period;
static volatile uint32_t bitseq_sample;

// forward declarations
void bitseq_clearResults(void);
uint8_t bitseq_bitAt(uint16_t bitNumber);
static void bitseq_CLK_PIN_intr_handler(int8_t key);
void bitseq_printBinary(uint32_t data, int untilBit);
void bitseq_printBufferBinary(int16_t msb, int16_t lsb);

#define bitseq_micros (0x7FFFFFFF & system_get_time())

void
bitseq_disableInterrupt(void) {
  //disable interrupt
  gpio_pin_intr_state_set(GPIO_ID_PIN(BITSEQ_CLK_PIN), GPIO_PIN_INTR_DISABLE);
  bitseq_results.statusBits &= ~BITSEQ_ISRUNNING;
}

void ICACHE_FLASH_ATTR
bitseq_enableInterrupt(void) {
  if (! (bitseq_results.statusBits & BITSEQ_INITIATED) ) {
    os_printf("Error: bitseq_enableInterrupt called before bitseq_init\n");
  } else {
    bitseq_clearResults();
    bitseq_results.statusBits |= BITSEQ_ISRUNNING;
    bitseq_results.statusBits &= ~BITSEQ_RESULT_IS_READY;
    bitseq_results.lastTimestamp = bitseq_micros;
    bitseq_results.noOfRestarts = 0;
    //enable interrupt
    if (bitseq_settings.onRising) {
      gpio_pin_intr_state_set(GPIO_ID_PIN(BITSEQ_CLK_PIN), GPIO_PIN_INTR_POSEDGE);
    } else {
      gpio_pin_intr_state_set(GPIO_ID_PIN(BITSEQ_CLK_PIN), GPIO_PIN_INTR_NEGEDGE);
    }
  }
}

void ICACHE_FLASH_ATTR
bitseq_clearResults(void){
  bitseq_results.currentBit = 0; // currentBit is truncated at BITSEQ_BUFFER_SIZE
  bitseq_results.sampledBits = 0; // bitseq_sampledBits is truncated at uint16_t
  bitseq_results.noOfRestarts = 0;
  bitseq_results.statusBits &= BITSEQ_INITIATED; // only keep initiated flag
  //bitseq_results.statusBits &= ~(BITSEQ_RESULT_IS_READY|BITSEQ_INTERRUPT_WHILE_NOT_RUNNING|
  //                        BITSEQ_INTERRUPT_WHILE_HAVE_RESULT|BITSEQ_NOT_ENOUGH_SILENCE|
  //                        BITSEQ_TOO_MUCH_SILENCE);
}

// quick and dirty implementation of sprintf with %f
int ICACHE_FLASH_ATTR
bitseq_float2string(float sample, int divisor, char *buf, int bufLen) {
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
bitseq_printBinary(uint32_t data, int untilBit) {
  int j;
  for ( j=untilBit; j>=0;j--){
    os_printf((data>>j)&1?"1":"0");
    if (j!=untilBit && (j%8)==0) {
      os_printf(" ");
    }
  }
}

void ICACHE_FLASH_ATTR
bitseq_printBinary8(uint8_t data) {
  bitseq_printBinary(data, 7);
}

void ICACHE_FLASH_ATTR
bitseq_printBinary16(uint16_t data){
  bitseq_printBinary(data, 15);
}

void ICACHE_FLASH_ATTR
bitseq_printBinary32(uint32_t data){
  bitseq_printBinary(data, 31);
}

/**
 * returns the bit value indicated by index
 */
uint8_t ICACHE_FLASH_ATTR
bitseq_bitAt(uint16_t bitNumber) {
  bitNumber &= BITSEQ_BUFFER_MASK;
  uint16_t bit  = bitNumber & 0x7;
  uint16_t byte = bitNumber >> 3;
  return (bitseq_results.data[byte] >> (bit)) &1;
}

/**
 * slice out bits from the bit stream and print.
 * bit numbering are inclusive.
 *
 * If msb or lsb are negative they will count backward from
 * the last sampled bit +1.
 * i.e. -1 will indicate the last sampled bit.
 */
void ICACHE_FLASH_ATTR
bitseq_printBufferBinary(int16_t msb, int16_t lsb) {
  if (msb<0) {
    msb = (bitseq_results.currentBit+msb) & BITSEQ_BUFFER_MASK;
  }
  if (lsb<0) {
    lsb = (bitseq_results.currentBit+lsb) & BITSEQ_BUFFER_MASK;
  }
  int bit = 0;
  int i = 0;
  if (msb < lsb){
    for (bit=msb; bit<=lsb; bit++, i++){
      os_printf(bitseq_bitAt(bit)?"1":"0");
      if ((i+1)%8==0) {
        os_printf(" ");
      }
    }
  } else {
    for (bit=msb; bit>=lsb; bit--, i++){
      os_printf(bitseq_bitAt(bit)?"1":"0");
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
 * the last sampled bit +1.
 * i.e. -1 will indicate the last sampled bit.
 */
uint32_t ICACHE_FLASH_ATTR
bitseq_sliceBits(int16_t msb, int16_t lsb, bool duplicateMsb){
  if (msb<0) {
    msb = bitseq_results.currentBit+msb;
  }
  if (lsb<0) {
    lsb = bitseq_results.currentBit+lsb;
  }
  uint32_t tmp = 0;
  int bit = 0;
  if (msb < lsb){
    if (duplicateMsb && bitseq_bitAt(msb)) {
      tmp = 0xFFFFFFFF;
    }
    for (bit=msb; bit<=lsb; bit++){
      tmp = tmp << 1;
      tmp |= bitseq_bitAt(bit);
    }
  } else {
    if (duplicateMsb && bitseq_bitAt(msb)) {
      tmp = 0xFFFFFFFF;
    }
    for (bit=msb; bit>=lsb; bit--){
      tmp = tmp << 1;
      tmp |= bitseq_bitAt(bit);
    }
  }
  return tmp;
}

/**
 * Store the bit value of 'bitseq_sample' in 'bitseq_results.data' at position 'bitseq_results.currentBit'
 * increments bitseq_results.currentBit.
 */
#define bitseq_storeBit {                    \
  uint16_t aByte = bitseq_results.currentBit >> 3; \
  uint16_t aBit = bitseq_results.currentBit & 0x7; \
  if (bitseq_sample) {                       \
    bitseq_results.data[aByte] |= 1<<aBit;         \
  } else {                                  \
    bitseq_results.data[aByte] &= ~(1 << aBit);    \
  }                                         \
  bitseq_results.currentBit = (bitseq_results.currentBit+1) & BITSEQ_BUFFER_MASK; \
}

/**
 * This sampler will start sampling directly.
 * When it detects the idle period it tests if enough bits have been acquired.
 * It will continue/restart sampling if not enough bits have been stored.
 */
static void
bitseq_CLK_PIN_intr_handler(int8_t key) {

  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  // clear interrupt status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));
  bitseq_now = bitseq_micros;
  bitseq_sample = GPIO_INPUT_GET(BITSEQ_DATA_PIN);
  bitseq_period = bitseq_now-bitseq_results.lastTimestamp;

  if (BITSEQ_RESULT_IS_READY & bitseq_results.statusBits) {
    return; // Should not happen
  }

  if (bitseq_period > bitseq_settings.minIdlePeriod){
    if ( bitseq_results.sampledBits >= bitseq_settings.numberOfBits) {
      // we're done
      bitseq_disableInterrupt();
      bitseq_results.statusBits |= BITSEQ_RESULT_IS_READY;
      os_timer_arm(&bitseq_callbackTimer, 1, 0);
    } else {
      // start over
      bitseq_results.currentBit = 0;
      bitseq_results.noOfRestarts += 1;
      bitseq_storeBit
      bitseq_results.sampledBits += 1;
    }
  } else {
    bitseq_storeBit
    bitseq_results.sampledBits += 1;
  }
  bitseq_results.lastTimestamp = bitseq_now;
}

volatile BitseqResult volatile* ICACHE_FLASH_ATTR
bitseq_getResult(void) {
  return &bitseq_results;
}

bool ICACHE_FLASH_ATTR
bitseq_isRunning(void) {
  bool rv = (bitseq_results.statusBits & BITSEQ_INITIATED) &&
            (bitseq_results.statusBits & BITSEQ_ISRUNNING);
  //os_printf("GPIOI: bitseq_isRunning");bitseq_printByteAsBinary(bitseq_results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

bool ICACHE_FLASH_ATTR
bitseq_isIdle(void) {
  bool rv = (bitseq_results.statusBits & BITSEQ_INITIATED) &&
            !(bitseq_results.statusBits & BITSEQ_ISRUNNING);
  //os_printf("GPIOI: bitseq_isRunning");bitseq_printByteAsBinary(bitseq_results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

bool ICACHE_FLASH_ATTR
bitseq_hasResults(void) {
  bool rv = (bitseq_results.statusBits & BITSEQ_INITIATED) &&
            (bitseq_results.statusBits & BITSEQ_RESULT_IS_READY);
  //os_printf("GPIOI: bitseq_isRunning");bitseq_printByteAsBinary(bitseq_results.statusBits);os_printf(" rv = %d\n", rv);
  return rv;
}

void ICACHE_FLASH_ATTR
bitseq_debugTrace(int16_t msb, int16_t lsb) {
  //os_printf("%02X%02X%02X%02X-",bitseq_results.data[0],bitseq_results.data[1],bitseq_results.data[2],bitseq_results.data[3]);
  bitseq_printBufferBinary(msb, lsb);
  os_printf(" %06X restarts:%d sampledB:%d currB:%d sb:",
    bitseq_sliceBits(msb, lsb, false),
    bitseq_results.noOfRestarts, // restarts = noOfRestarts
    bitseq_results.sampledBits,  // sampledB = sampledBits
    bitseq_results.currentBit   // cb = current bit
    );
  bitseq_printBinary16(bitseq_results.statusBits);
  os_printf("\n\r");
}

/**
 * numberOfBits: the number of bits we are going to sample
 * minIdlePeriod: at least this many us between blocks
 * onRising : rising or falling edge trigger
 * resultCb : the callback to call when results are available
 */
void ICACHE_FLASH_ATTR
bitseq_init(uint16_t numberOfBits, uint32_t minIdlePeriod, bool onRising, os_timer_func_t *resultCb) {

  //Setup callback timer
  os_timer_disarm(&bitseq_callbackTimer);
  os_timer_setfn(&bitseq_callbackTimer, resultCb, NULL);

  bitseq_settings.minIdlePeriod = minIdlePeriod;
  bitseq_settings.numberOfBits = numberOfBits;
  bitseq_settings.onRising = onRising;

  bitseq_results.data = (uint8_t*)os_malloc(BITSEQ_BUFFER_SIZE);
  bitseq_results.statusBits = 0;

  bitseq_clearResults();

  if (0 == BITSEQ_CLK_PIN){
    //set gpio0 as gpio pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    //disable pull downs
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO0_U);
    //disable pull ups
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);
    // disable output
    GPIO_DIS_OUTPUT(BITSEQ_CLK_PIN);
    ETS_GPIO_INTR_ATTACH(bitseq_CLK_PIN_intr_handler,0);
    ETS_GPIO_INTR_DISABLE();
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
    gpio_output_set(0, 0, 0, GPIO_ID_PIN(BITSEQ_CLK_PIN));
    gpio_register_set(GPIO_PIN_ADDR(0), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
        | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
        | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
  } else {
    os_printf("bitseq_init: Error BITSEQ_CLK_PIN==%d is not implemented", BITSEQ_CLK_PIN);
    return;
  }

  if (2 == BITSEQ_DATA_PIN){
    //set gpio2 as gpio pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
    //disable pull downs
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO2_U);
    //disable pull ups
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
    // disable output
    GPIO_DIS_OUTPUT(BITSEQ_DATA_PIN);
  } else if (3 == BITSEQ_DATA_PIN) {
    //set gpio2 as gpio pin
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
    //disable pull downs
    PIN_PULLDWN_DIS(PERIPHS_IO_MUX_U0RXD_U);
    //disable pull ups
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);
    // disable output
    GPIO_DIS_OUTPUT(BITSEQ_DATA_PIN);
  } else {
    os_printf("bitseq_init: Error BITSEQ_DATA_PIN==%d is not implemented", BITSEQ_DATA_PIN);
    return;
  }
  os_printf("bitseq_init: Initiated the GPIOI sampler with BITSEQ_CLK_PIN=%d and BITSEQ_DATA_PIN=%d.", BITSEQ_CLK_PIN, BITSEQ_DATA_PIN);
  os_printf("bitseq_init: Will sample %d bits on the %s edge", bitseq_settings.numberOfBits, bitseq_settings.onRising?"rising":"falling");

  //clear gpio status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(0));
  bitseq_disableInterrupt();
  ETS_GPIO_INTR_ENABLE();

  bitseq_results.statusBits |= BITSEQ_INITIATED;
}

