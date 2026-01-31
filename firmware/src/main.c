#include <string.h>
#include <stdio.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "st25dv.h"

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

static uint8_t st25_addr7(uint16_t st_addr8) { return (uint8_t)(st_addr8 >> 1); }

static int32_t st25_bus_init(void) {
  i2c_setup();
  return 0;
}

static int32_t st25_bus_deinit(void) { return 0; }

static uint32_t st25_get_tick(void) { return (uint32_t)to_ms_since_boot(get_absolute_time()); }

static int32_t st25_is_ready(uint16_t dev_addr, const uint32_t trials) {
  const uint8_t addr7 = st25_addr7(dev_addr);
  for (uint32_t i = 0; i < trials; i++) {
    if (i2c_address_acks(addr7)) {
      return NFCTAG_OK;
    }
    sleep_ms(2);
  }
  return NFCTAG_NACK;
}

static int32_t st25_write(uint16_t dev_addr, uint16_t reg, const uint8_t* data, uint16_t len) {
  const uint8_t addr7 = st25_addr7(dev_addr);

  uint8_t tmp[2 + ST25DV_MAX_WRITE_BYTE];
  if (len > ST25DV_MAX_WRITE_BYTE) {
    return NFCTAG_ERROR;
  }

  tmp[0] = (uint8_t)((reg >> 8) & 0xFF);
  tmp[1] = (uint8_t)(reg & 0xFF);
  if (len > 0) {
    memcpy(&tmp[2], data, len);
  }

  const int res = i2c_write_blocking(NFC_I2C_INSTANCE, addr7, tmp, (int)(len + 2), false);
  return (res < 0) ? NFCTAG_NACK : NFCTAG_OK;
}

static int32_t st25_read(uint16_t dev_addr, uint16_t reg, uint8_t* data, uint16_t len) {
  const uint8_t addr7 = st25_addr7(dev_addr);

  const uint8_t addr_buf[2] = {
      (uint8_t)((reg >> 8) & 0xFF),
      (uint8_t)(reg & 0xFF),
  };

  int res = i2c_write_blocking(NFC_I2C_INSTANCE, addr7, addr_buf, 2, true);
  if (res < 0) {
    return NFCTAG_NACK;
  }

  res = i2c_read_blocking(NFC_I2C_INSTANCE, addr7, data, len, false);
  return (res < 0) ? NFCTAG_NACK : NFCTAG_OK;
}

static void dump_hex(const uint8_t* buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    printf("%02X", buf[i]);
    if ((i + 1) != len) {
      printf(" ");
    }
  }
}

static bool is_inki_request(const uint8_t req16[16]) {
  return req16[0] == 'I' && req16[1] == 'N' && req16[2] == 'K' && req16[3] == 'I';
}

int main(void) {
  stdio_init_all();
  sleep_ms(1500);

  printf("\nNFC harness (RP2040/Pico) boot\n");
  printf("I2C: SDA=GP%u SCL=GP%u @ %u Hz\n",
         NFC_I2C_SDA_PIN,
         NFC_I2C_SCL_PIN,
         (unsigned)NFC_I2C_BAUDRATE_HZ);

  gpio_init(NFC_STATUS_LED_PIN);
  gpio_set_dir(NFC_STATUS_LED_PIN, GPIO_OUT);
  gpio_put(NFC_STATUS_LED_PIN, 0);

  ST25DV_Object_t st = {0};
  ST25DV_IO_t io = {
      .Init = st25_bus_init,
      .DeInit = st25_bus_deinit,
      .IsReady = st25_is_ready,
      .Write = st25_write,
      .Read = st25_read,
      .GetTick = st25_get_tick,
  };

  int32_t ret = ST25DV_RegisterBusIO(&st, &io);
  if (ret != NFCTAG_OK) {
    printf("ST25DV_RegisterBusIO failed: %ld\n", (long)ret);
    while (true) {
      gpio_xor_mask(1u << NFC_STATUS_LED_PIN);
      sleep_ms(250);
    }
  }

  ret = St25Dv_Drv.Init(&st);
  if (ret != NFCTAG_OK) {
    printf("St25Dv_Drv.Init failed: %ld\n", (long)ret);
  }

  uint8_t icref = 0;
  ret = St25Dv_Drv.ReadID(&st, &icref);
  if (ret == NFCTAG_OK) {
    printf("ICREF: 0x%02X\n", icref);
  } else {
    printf("St25Dv_Drv.ReadID failed: %ld\n", (long)ret);
  }

  ST25DV_MEM_SIZE mem = {0};
  ret = ST25DV_ReadMemSize(&st, &mem);
  if (ret != NFCTAG_OK) {
    printf("ReadMemSize failed: %ld\n", (long)ret);
  }

  const uint32_t num_blocks = (uint32_t)mem.Mem_Size + 1;
  const uint32_t bytes_per_block = (uint32_t)mem.BlockSize + 1;
  const uint32_t total_bytes = num_blocks * bytes_per_block;
  printf("Memory: blocks=%lu bytes_per_block=%lu total=%lu bytes\n",
         (unsigned long)num_blocks,
         (unsigned long)bytes_per_block,
         (unsigned long)total_bytes);
  printf("Expected for ST25DV (7-bit): 0x53 (user/dynamic), 0x57 (system)\n");

  while (true) {
    gpio_put(NFC_STATUS_LED_PIN, 1);

    if (total_bytes < 16) {
      printf("Memory too small for request check\n");
      sleep_ms(1000);
      continue;
    }

    const uint32_t blocks_needed = (16u + bytes_per_block - 1u) / bytes_per_block;
    const uint32_t region_bytes = blocks_needed * bytes_per_block;
    if (region_bytes > total_bytes) {
      printf("Invalid request region: %lu > %lu\n", (unsigned long)region_bytes, (unsigned long)total_bytes);
      sleep_ms(1000);
      continue;
    }

    const uint16_t req_addr = (uint16_t)(total_bytes - region_bytes);
    uint8_t req[16] = {0};

    ret = st25_is_ready(ST25DV_ADDR_DATA_I2C, 1);
    if (ret != NFCTAG_OK) {
      printf("ST25DV not ready (RF busy?): %ld\n", (long)ret);
      gpio_put(NFC_STATUS_LED_PIN, 0);
      sleep_ms(200);
      continue;
    }

    ret = St25Dv_Drv.ReadData(&st, req, req_addr, (uint16_t)sizeof(req));
    if (ret != NFCTAG_OK) {
      printf("ReadData failed: %ld\n", (long)ret);
      gpio_put(NFC_STATUS_LED_PIN, 0);
      sleep_ms(500);
      continue;
    }

    printf("Request@0x%04X: ", req_addr);
    dump_hex(req, sizeof(req));
    printf("\n");

    if (is_inki_request(req)) {
      const uint8_t version = req[4];
      const uint8_t opcode = req[5];
      const uint16_t duration_min = (uint16_t)req[6] | ((uint16_t)req[7] << 8);
      const uint32_t unix_seconds = (uint32_t)req[8] | ((uint32_t)req[9] << 8) |
                                    ((uint32_t)req[10] << 16) | ((uint32_t)req[11] << 24);
      const uint32_t nonce = (uint32_t)req[12] | ((uint32_t)req[13] << 8) | ((uint32_t)req[14] << 16) |
                             ((uint32_t)req[15] << 24);

      printf("INKI request: version=%u opcode=0x%02X duration_min=%u unix=%lu nonce=0x%08lX\n",
             version,
             opcode,
             duration_min,
             (unsigned long)unix_seconds,
             (unsigned long)nonce);

      uint8_t zeros[ST25DV_MAX_WRITE_BYTE] = {0};
      ret = St25Dv_Drv.WriteData(&st, zeros, req_addr, (uint16_t)region_bytes);
      if (ret == NFCTAG_OK) {
        printf("Cleared request (%lu bytes @ 0x%04X)\n", (unsigned long)region_bytes, req_addr);
      } else {
        printf("Clear failed: %ld\n", (long)ret);
      }
    }

    gpio_put(NFC_STATUS_LED_PIN, 0);
    sleep_ms(1000);
  }
}
