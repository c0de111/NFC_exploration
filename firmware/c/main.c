#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#ifndef NFC_I2C_INSTANCE
#define NFC_I2C_INSTANCE i2c0
#endif

#ifndef NFC_I2C_BAUDRATE_HZ
#define NFC_I2C_BAUDRATE_HZ 100000
#endif

#ifndef NFC_I2C_SDA_PIN
#define NFC_I2C_SDA_PIN 4
#endif

#ifndef NFC_I2C_SCL_PIN
#define NFC_I2C_SCL_PIN 5
#endif

#ifndef NFC_STATUS_LED_PIN
#define NFC_STATUS_LED_PIN 15
#endif

static void i2c_setup(void) {
  i2c_init(NFC_I2C_INSTANCE, NFC_I2C_BAUDRATE_HZ);
  gpio_set_function(NFC_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(NFC_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(NFC_I2C_SDA_PIN);
  gpio_pull_up(NFC_I2C_SCL_PIN);
}

static bool i2c_address_acks(uint8_t address_7bit) {
  int res = i2c_write_blocking(NFC_I2C_INSTANCE, address_7bit, NULL, 0, false);
  return res >= 0;
}

static void scan_i2c_bus_once(void) {
  printf("I2C scan on SDA=GP%u SCL=GP%u @ %u Hz\n",
         NFC_I2C_SDA_PIN,
         NFC_I2C_SCL_PIN,
         (unsigned)NFC_I2C_BAUDRATE_HZ);

  bool any = false;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (i2c_address_acks(addr)) {
      printf("  - device @ 0x%02X\n", addr);
      any = true;
    }
  }
  if (!any) {
    printf("  (no devices found)\n");
  } else {
    printf("Expected for ST25DV (7-bit): 0x53 (user/dynamic), 0x57 (system)\n");
  }
}

int main(void) {
  stdio_init_all();
  sleep_ms(1500);

  printf("\nNFC harness (RP2040/Pico) boot\n");

  gpio_init(NFC_STATUS_LED_PIN);
  gpio_set_dir(NFC_STATUS_LED_PIN, GPIO_OUT);
  gpio_put(NFC_STATUS_LED_PIN, 0);

  i2c_setup();

  while (true) {
    scan_i2c_bus_once();
    for (int i = 0; i < 10; i++) {
      gpio_xor_mask(1u << NFC_STATUS_LED_PIN);
      sleep_ms(100);
    }
    gpio_put(NFC_STATUS_LED_PIN, 0);
    sleep_ms(1000);
  }
}

