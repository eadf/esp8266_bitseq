/*
 * max6675.c
 *
 *  This is a direct port of the Arduino library found at https://github.com/adafruit/MAX6675-library
 *
 * this library is public domain. enjoy!
 * www.ladyada.net/learn/sensors/thermocouple
 */
#include "driver/max6675.h"
#include "gpio.h"
#include "ets_sys.h"
#include "osapi.h"
#include "math.h"
#include "driver/dro_utils.h"

#define CS_PIN 0    // This is GIOP0, connect a 10K pullup to Vcc to avoid problems with the bootloader starting
#define SCLK_PIN 2  // This is GIOP2, connect a 10K pulldown to GND to avoid problems with the bootloader starting
#define MISO_PIN 3  // this pin is normally RX. So don't have your hardware connected while flashing.

void max6675_delay(uint32 microseconds);
void max6675_delay_between_clock(void);
void max6675_digitalWrite(unsigned int pin, bool value);
bool max6675_digitalRead(unsigned int pin);
uint8_t max6675_readByte(void);

// some arduino lookalike methods :)
#define LOW false
#define HIGH true

void ICACHE_FLASH_ATTR
max6675_delay(uint32 microseconds) {
  os_delay_us(1000*microseconds);
}

void ICACHE_FLASH_ATTR
max6675_delay_between_clock(void) {
  os_delay_us(500);
}

void ICACHE_FLASH_ATTR
max6675_digitalWrite(unsigned int pin, bool value) {
  GPIO_OUTPUT_SET(pin, value);
}

bool ICACHE_FLASH_ATTR
max6675_digitalRead(unsigned int pin) {
  return GPIO_INPUT_GET(pin)!=0;
}

/******************************************************************************
 * FunctionName : max6675_init
 * Description  : initialize I2C bus to enable faked i2c operations
 * IMPORTANT    : Make sure that the UART/serial hardware isn't using the RX pin!!
 * Parameters   : NONE
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
max6675_init(void) {
  //set gpio3 as gpio pin CS_PIN
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_GPIO3);
  //set gpio2 as gpio pin SCLK_PIN
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
  //set gpio0 as gpio pin MISO_PIN
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);

  //disable pull downs
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO2_U);
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_GPIO0_U);
  PIN_PULLDWN_DIS(PERIPHS_IO_MUX_U0RXD_U);

  //disable pull ups
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO2_U);
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_GPIO0_U);
  PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0RXD_U);

  max6675_digitalWrite(CS_PIN, true);
  max6675_digitalWrite(SCLK_PIN, false);
  GPIO_DIS_OUTPUT(MISO_PIN);
}

uint8_t ICACHE_FLASH_ATTR
max6675_readByte(void) {
  int i;
  uint8_t d = 0;
  for (i=7; i>=0; i--)
  {
    max6675_digitalWrite(SCLK_PIN, LOW);
    max6675_delay_between_clock();
    if (max6675_digitalRead(MISO_PIN)) {
      //set the bit to 0 no matter what
      d |= (1 << i);
    }
    max6675_digitalWrite(SCLK_PIN, HIGH);
    max6675_delay_between_clock();
  }
  return d;
}

bool ICACHE_FLASH_ATTR
max6675_readTemp(float* sample, bool celcius)
{
  //max6675_init(); // just a test

  uint16_t v;
  max6675_digitalWrite(CS_PIN, LOW);
  max6675_delay_between_clock();
  v = max6675_readByte();
  v <<= 8;
  v |= max6675_readByte();
  max6675_digitalWrite(SCLK_PIN, LOW);
  max6675_delay_between_clock();
  max6675_digitalWrite(CS_PIN, HIGH);
  if (v & 0x4) {
    // uh oh, no thermocouple attached!
    os_printf("The max6675 thinks the thermocouple is lose.\r\n", v);
    *sample = NAN;
    return false;
  }
  v >>= 3;
  *sample = v*0.25;
  if(!celcius) {
    *sample *= 9.0/5.0 + 32.0;
  }
  os_printf("max6675_readTemp got result %d/100 \r\n" , (int)((*sample)*100.0));
  return true;
}

bool ICACHE_FLASH_ATTR
max6675_readTempAsString(char *buf, int bufLen, int *bytesWritten, bool celcius){
  float sample = 0.0;
  bool rv = max6675_readTemp(&sample, celcius);
  if(rv){
    *bytesWritten = dro_utils_float_2_string(100.0*sample, 100, buf, bufLen);
  } else {
    *bytesWritten = 0;
    buf[0] = 0;
  }
  return rv;
}
