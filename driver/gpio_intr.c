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

static const uint16_t GPIOI_CLK_PIN = 0;
static const uint16_t GPIOI_DATA_PIN = 2;

// wow, we love static global data, don't we?

static volatile uint32_t GPIOI_maxClockPeriod = 0;
static volatile uint32_t GPIOI_minStartPeriod = 0;
static volatile uint32_t GPIOI_maxStartPeriod = 0;
static volatile uint16_t GPIOI_numberOfBits = 24;
static volatile bool GPIOI_onRising = false;

static volatile bool GPIOI_everStarted = false;
static volatile bool GPIOI_resultReady = false;
static volatile uint32_t GPIOI_data = 0;
static volatile uint32_t GPIOI_lastTimestamp = 0;
static volatile uint32_t GPIOI_fastest = 0;
static volatile uint32_t GPIOI_slowest = 0;
static volatile uint32_t GPIOI_bzp = 0;
static volatile uint8_t GPIOI_statusBits = 0;


static volatile uint16_t GPIOI_currentBit = 0;

static volatile uint32_t GPIOI_heartbeat = 0;

void ICACHE_FLASH_ATTR
GPIOI_disableInterrupt(void) {
  //disable interrupt
  gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_DISABLE);
}

uint32_t ICACHE_FLASH_ATTR GPIOI_getHeartbeat(void) {
  return GPIOI_heartbeat;
}

void ICACHE_FLASH_ATTR
GPIOI_enableInterrupt(void) {
  GPIOI_statusBits = 0;
  GPIOI_everStarted = true;
  GPIOI_currentBit = 0;
  GPIOI_data = 0;
  GPIOI_fastest = 0xFFFFFFFF;
  GPIOI_slowest = 0;
  GPIOI_resultReady = false;
  //enable interrupt
  if (GPIOI_onRising) {
    gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_POSEDGE);
  } else {
    gpio_pin_intr_state_set(GPIO_ID_PIN(GPIOI_CLK_PIN), GPIO_PIN_INTR_NEGEDGE);
  }
}

uint32_t ICACHE_FLASH_ATTR
GPIOI_micros(void) {
  return 0x7FFFFFFF & system_get_time();
}

void ICACHE_FLASH_ATTR
GPIOIprintBinary(uint32_t data){
  int i = 0;
  for (i=8*sizeof(uint32_t)-1; i>=0; i--){
    if (i > 1 && (i+1)%4==0) os_printf(" ");
    if (data & 1<<i){
      os_printf("1");
    } else {
      os_printf("0");
    }
  }
}

LOCAL void
GPIOI_CLK_PIN_intr_handler(int8_t key)
{
  uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
  //clear interrupt status
  GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(0));

#ifdef DRIVER_GPIO_INTR_ENABLE_HEARTBEAT
  GPIOI_heartbeat +=1;
#endif

  if (GPIOI_resultReady) {
    GPIOI_statusBits |= GPIOI_ALREADY_DONE;
    return; // we are already done here
  }

  uint32_t now = GPIOI_micros();
  uint32_t period = now-GPIOI_lastTimestamp;

  if (GPIOI_currentBit==0){
    if (period < GPIOI_minStartPeriod) {
      // There were not enough silencebefore this bit, start over
      GPIOI_statusBits |= GPIOI_NOT_ENOUGH_SILENCE;
      GPIOI_lastTimestamp = now;
      return;
    }
    if ( period > GPIOI_maxStartPeriod){
      // There were too much silence before this bit, start over
      GPIOI_statusBits |= GPIOI_TOO_MUCH_SILENCE;
      GPIOI_lastTimestamp = now;
      return;
    }
    GPIOI_bzp = period;
    GPIOI_data = 0;
    GPIOI_fastest = 0xFFFFFFFF;
    GPIOI_slowest = 0;
    GPIOI_bzp = period;
  } else {
    if (period > GPIOI_maxClockPeriod){
      // reset timing and start over
      GPIOI_currentBit = 0;
    } else {
      if (period < GPIOI_fastest) {
        GPIOI_fastest = period;
      }
      if (period > GPIOI_slowest){
        GPIOI_slowest = period;
      }
    }
  }
  GPIOI_lastTimestamp = now;
  GPIOI_data |= GPIO_INPUT_GET(GPIOI_DATA_PIN)<<GPIOI_currentBit;
  GPIOI_currentBit += 1;

  if (GPIOI_currentBit >= GPIOI_numberOfBits){
    GPIOI_disableInterrupt();
    GPIOI_resultReady = true;
  }
}

uint32_t ICACHE_FLASH_ATTR
GPIOI_getResult(uint32_t *fastestPeriod, uint32_t *slowestPeriod, uint32_t *bitZeroPeriod, uint8_t *statusBits,  uint16_t *currentBit){
  *fastestPeriod = GPIOI_fastest;
  *slowestPeriod = GPIOI_slowest;
  *currentBit = GPIOI_currentBit;
  *bitZeroPeriod = GPIOI_bzp;
  *statusBits = GPIOI_statusBits;

  return GPIOI_data;
}

bool GPIOI_isRunning() {
  return GPIOI_everStarted && !GPIOI_resultReady;
}

/**
 * numberOfBits: the number of bits we are going to sample
 * maxClockPeriod: the maximum length of a clock period
 * minStartPeriod: at least this many us between blocks
 * maxStartPeriod: at most this many us between blocks
 */
void ICACHE_FLASH_ATTR
GPIOI_init(uint16_t numberOfBits, uint32_t maxClockPeriod, uint32_t minStartPeriod, uint32_t maxStartPeriod, bool onRising)
{
  GPIOI_maxClockPeriod = maxClockPeriod;
  GPIOI_minStartPeriod = minStartPeriod;
  GPIOI_maxStartPeriod = maxStartPeriod;
  GPIOI_numberOfBits = numberOfBits;
  GPIOI_onRising = onRising;

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
}

