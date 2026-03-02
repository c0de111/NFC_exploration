#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "hardware/adc.h"
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
#define NFC_I2C_SDA_PIN 20
#endif

#ifndef NFC_I2C_SCL_PIN
#define NFC_I2C_SCL_PIN 21
#endif

#ifndef NFC_POWER_HOLD_PIN
#define NFC_POWER_HOLD_PIN 28
#endif

#ifndef NFC_ST25_VCC_EN_PIN
#define NFC_ST25_VCC_EN_PIN 18
#endif

#ifndef NFC_ST25_POWER_ON_DELAY_MS
#define NFC_ST25_POWER_ON_DELAY_MS 10
#endif

#ifndef NFC_I2C_OP_TIMEOUT_US
#define NFC_I2C_OP_TIMEOUT_US 20000
#endif

#ifndef NFC_TUNE_ADC_INPUT
#define NFC_TUNE_ADC_INPUT 1
#endif

#ifndef NFC_TUNE_SAMPLE_COUNT
#define NFC_TUNE_SAMPLE_COUNT 256
#endif

#ifndef NFC_TUNE_REPORT_INTERVAL_MS
#define NFC_TUNE_REPORT_INTERVAL_MS 100
#endif

#ifndef NFC_TUNE_SAMPLE_DELAY_US
#define NFC_TUNE_SAMPLE_DELAY_US 20
#endif

#ifndef NFC_TUNE_EMA_ALPHA
#define NFC_TUNE_EMA_ALPHA 0.50f
#endif

#ifndef NFC_TUNE_BASELINE_SAMPLES
#define NFC_TUNE_BASELINE_SAMPLES 4000
#endif

#ifndef NFC_TUNE_FIELD_ON_MV
#define NFC_TUNE_FIELD_ON_MV 9.0f
#endif

#ifndef NFC_TUNE_FIELD_OFF_MV
#define NFC_TUNE_FIELD_OFF_MV 4.0f
#endif

#ifndef NFC_TUNE_DEBOUNCE_COUNT
#define NFC_TUNE_DEBOUNCE_COUNT 1
#endif

#ifndef NFC_TUNE_BASELINE_TRACK_ALPHA
#define NFC_TUNE_BASELINE_TRACK_ALPHA 0.01f
#endif

#ifndef NFC_TUNE_FREEZE_BASELINE
#define NFC_TUNE_FREEZE_BASELINE 1
#endif

#ifndef NFC_TUNE_GRAPH_WIDTH
#define NFC_TUNE_GRAPH_WIDTH 72
#endif

#ifndef NFC_TUNE_GRAPH_HEIGHT
#define NFC_TUNE_GRAPH_HEIGHT 20
#endif

#ifndef NFC_TUNE_GRAPH_MAX_MV
#define NFC_TUNE_GRAPH_MAX_MV 3000.0f
#endif

#ifndef NFC_TUNE_BASELINE_SETTLE_MS
#define NFC_TUNE_BASELINE_SETTLE_MS 400
#endif

#ifndef NFC_TUNE_EH_ENFORCE_RETRIES
#define NFC_TUNE_EH_ENFORCE_RETRIES 8
#endif

#ifndef NFC_TUNE_EH_ENFORCE_RETRY_MS
#define NFC_TUNE_EH_ENFORCE_RETRY_MS 20
#endif

#ifndef NFC_TUNE_EH_RECHECK_MS
#define NFC_TUNE_EH_RECHECK_MS 1000
#endif

#define COLOR_RESET "\033[0m"
#define COLOR_CYAN "\033[36m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_RED "\033[31m"
#define COLOR_DIM "\033[2m"
#define COLOR_BOLD_GREEN "\033[1;32m"
#define COLOR_BOLD_YELLOW "\033[1;33m"
#define COLOR_BOLD_RED "\033[1;31m"
#define TERM_HIDE_CURSOR "\033[?25l"

static void i2c_setup(void) {
  i2c_init(NFC_I2C_INSTANCE, NFC_I2C_BAUDRATE_HZ);
  gpio_set_function(NFC_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(NFC_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(NFC_I2C_SDA_PIN);
  gpio_pull_up(NFC_I2C_SCL_PIN);
}

static void init_power_hold_early(void) {
#if (NFC_POWER_HOLD_PIN >= 0)
  gpio_init(NFC_POWER_HOLD_PIN);
  gpio_set_dir(NFC_POWER_HOLD_PIN, GPIO_OUT);
  gpio_put(NFC_POWER_HOLD_PIN, 1);
#endif
}

static void st25_power_on(void) {
#if (NFC_ST25_VCC_EN_PIN >= 0)
  gpio_init(NFC_ST25_VCC_EN_PIN);
  gpio_set_dir(NFC_ST25_VCC_EN_PIN, GPIO_OUT);
  gpio_put(NFC_ST25_VCC_EN_PIN, 1);
#endif
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
  const uint8_t probe_addr[2] = {0x00, 0x00};

  for (uint32_t i = 0; i < trials; i++) {
    const int res = i2c_write_timeout_us(
        NFC_I2C_INSTANCE, addr7, probe_addr, (size_t)sizeof(probe_addr), false, NFC_I2C_OP_TIMEOUT_US);
    if (res == (int)sizeof(probe_addr)) {
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
  for (uint16_t i = 0; i < len; i++) {
    tmp[2 + i] = data[i];
  }

  const int res = i2c_write_timeout_us(NFC_I2C_INSTANCE, addr7, tmp, (size_t)(len + 2), false, NFC_I2C_OP_TIMEOUT_US);
  return (res < 0) ? NFCTAG_NACK : NFCTAG_OK;
}

static int32_t st25_read(uint16_t dev_addr, uint16_t reg, uint8_t* data, uint16_t len) {
  const uint8_t addr7 = st25_addr7(dev_addr);
  const uint8_t addr_buf[2] = {
      (uint8_t)((reg >> 8) & 0xFF),
      (uint8_t)(reg & 0xFF),
  };

  int res = i2c_write_timeout_us(NFC_I2C_INSTANCE, addr7, addr_buf, sizeof(addr_buf), true, NFC_I2C_OP_TIMEOUT_US);
  if (res < 0) {
    return NFCTAG_NACK;
  }

  res = i2c_read_timeout_us(NFC_I2C_INSTANCE, addr7, data, len, false, NFC_I2C_OP_TIMEOUT_US);
  return (res < 0) ? NFCTAG_NACK : NFCTAG_OK;
}

static bool tick_elapsed(uint32_t now_ms, uint32_t deadline_ms) { return ((int32_t)(now_ms - deadline_ms) >= 0); }

static bool st25_enforce_eh_enabled(ST25DV_Object_t* st, bool verbose, ST25DV_EH_CTRL* out_state) {
  ST25DV_EH_CTRL eh = {0};
  int32_t ret = NFCTAG_OK;

  for (uint32_t attempt = 0; attempt < (uint32_t)NFC_TUNE_EH_ENFORCE_RETRIES; attempt++) {
    ret = ST25DV_ReadEHCtrl_Dyn(st, &eh);
    if (ret == NFCTAG_OK && eh.EH_EN_Mode == ST25DV_ENABLE) {
      if (out_state != NULL) {
        *out_state = eh;
      }
      if (verbose && attempt > 0u) {
        printf("[OK] EH enable latched after %lu retry step(s)\n", (unsigned long)attempt);
      }
      return true;
    }

    ret = ST25DV_SetEHENMode_Dyn(st);
    if (ret != NFCTAG_OK) {
      // Datasheet behavior: writing EH_MODE=ACTIVE_AFTER_BOOT should force EH_EN=1.
      (void)ST25DV_WriteEHMode(st, ST25DV_EH_ACTIVE_AFTER_BOOT);
    }
    sleep_ms(NFC_TUNE_EH_ENFORCE_RETRY_MS);
  }

  ret = ST25DV_ReadEHCtrl_Dyn(st, &eh);
  if (ret == NFCTAG_OK) {
    if (out_state != NULL) {
      *out_state = eh;
    }
    if (verbose) {
      printf("[WARN] EH enable not latched: EH_EN=%u EH_ON=%u FIELD_ON=%u VCC_ON=%u\n",
             (unsigned)eh.EH_EN_Mode,
             (unsigned)eh.EH_on,
             (unsigned)eh.Field_on,
             (unsigned)eh.VCC_on);
    }
  } else if (verbose) {
    printf("[WARN] EH state read failed after enforce attempts: %ld\n", (long)ret);
  }
  return false;
}

static bool st25_rearm_eh_path(ST25DV_Object_t* st, bool verbose, ST25DV_EH_CTRL* out_state) {
  (void)ST25DV_ResetEHENMode_Dyn(st);
  sleep_ms(2);
  (void)ST25DV_WriteEHMode(st, ST25DV_EH_ACTIVE_AFTER_BOOT);
  sleep_ms(2);
  return st25_enforce_eh_enabled(st, verbose, out_state);
}

static bool st25_enable_eh_mode(ST25DV_Object_t* st) {
  bool ok = true;
  int32_t ret = NFCTAG_OK;

  ST25DV_EH_MODE_STATUS eh_mode = ST25DV_EH_ACTIVE_AFTER_BOOT;
  ret = ST25DV_ReadEHMode(st, &eh_mode);
  if (ret == NFCTAG_OK) {
    printf("[INFO] EH mode before: %s\n", (eh_mode == ST25DV_EH_ACTIVE_AFTER_BOOT) ? "ACTIVE_AFTER_BOOT" : "ON_DEMAND");
  } else {
    printf("[WARN] EH mode read failed: %ld\n", (long)ret);
    ok = false;
  }

  ret = ST25DV_WriteEHMode(st, ST25DV_EH_ACTIVE_AFTER_BOOT);
  if (ret == NFCTAG_OK) {
    printf("[OK] EH mode write: ACTIVE_AFTER_BOOT\n");
  } else {
    printf("[ERR] EH mode write failed: %ld\n", (long)ret);
    ok = false;
  }

  ST25DV_EH_CTRL eh = {0};
  const bool eh_latched = st25_enforce_eh_enabled(st, true, &eh);
  if (eh_latched) {
    printf("[OK] EH dynamic enable latched\n");
  } else {
    printf("[WARN] EH dynamic enable did not latch, trying rearm sequence\n");
    const bool eh_latched_after_rearm = st25_rearm_eh_path(st, true, &eh);
    if (eh_latched_after_rearm) {
      printf("[OK] EH dynamic enable latched after rearm\n");
    } else {
      ok = false;
    }
  }

  ret = ST25DV_ReadEHCtrl_Dyn(st, &eh);
  if (ret == NFCTAG_OK) {
    printf("[INFO] EH state: EH_EN=%u EH_ON=%u FIELD_ON=%u VCC_ON=%u\n",
           (unsigned)eh.EH_EN_Mode,
           (unsigned)eh.EH_on,
           (unsigned)eh.Field_on,
           (unsigned)eh.VCC_on);
  } else {
    printf("[WARN] EH state read failed: %ld\n", (long)ret);
    ok = false;
  }

  return ok;
}

static int adc_input_to_gpio(uint adc_input) {
  if (adc_input > 3u) {
    return -1;
  }
  return (int)(26u + adc_input);
}

static float adc_raw_to_v(float raw) { return raw * (3.3f / 4095.0f); }

static uint16_t sample_adc_raw_once(uint adc_input) {
  adc_select_input(adc_input);
  sleep_us(2);
  (void)adc_read();
  sleep_us(2);
  return adc_read();
}

static float sample_adc_raw_average(uint adc_input, uint32_t sample_count) {
  if (sample_count == 0u) {
    return (float)sample_adc_raw_once(adc_input);
  }

  uint64_t sum = 0u;
  for (uint32_t i = 0; i < sample_count; i++) {
    sum += sample_adc_raw_once(adc_input);
#if (NFC_TUNE_SAMPLE_DELAY_US > 0)
    sleep_us(NFC_TUNE_SAMPLE_DELAY_US);
#endif
  }
  return (float)sum / (float)sample_count;
}

static bool st25_read_eh_ctrl_quick(ST25DV_Object_t* st, ST25DV_EH_CTRL* out) {
  int32_t ret = ST25DV_ReadEHCtrl_Dyn(st, out);
  if (ret == NFCTAG_OK) {
    return true;
  }
  sleep_ms(2);
  ret = ST25DV_ReadEHCtrl_Dyn(st, out);
  return ret == NFCTAG_OK;
}

static uint32_t graph_bar_height(float mv, float max_mv, uint32_t height) {
  if (height == 0u || max_mv <= 0.0f) {
    return 0u;
  }
  if (mv < 0.0f) {
    mv = 0.0f;
  }
  if (mv > max_mv) {
    mv = max_mv;
  }
  uint32_t bar_h = (uint32_t)((mv * (float)height / max_mv) + 0.5f);
  if (bar_h > height) {
    bar_h = height;
  }
  return bar_h;
}

static int32_t graph_row_for_mv(float mv, float max_mv, uint32_t height) {
  if (height == 0u || max_mv <= 0.0f) {
    return -1;
  }
  if (mv < 0.0f) {
    mv = 0.0f;
  }
  if (mv > max_mv) {
    mv = max_mv;
  }
  float frac = mv / max_mv;
  int32_t row = (int32_t)((1.0f - frac) * (float)(height - 1u) + 0.5f);
  if (row < 0) {
    row = 0;
  }
  if (row >= (int32_t)height) {
    row = (int32_t)height - 1;
  }
  return row;
}

static const char* graph_color_for_row(uint32_t row, uint32_t height) {
  if (height <= 1u) {
    return COLOR_GREEN;
  }
  const float frac = 1.0f - ((float)row / (float)(height - 1u));
  if (frac > 0.85f) {
    return COLOR_BOLD_RED;
  }
  if (frac > 0.65f) {
    return COLOR_BOLD_YELLOW;
  }
  if (frac > 0.45f) {
    return COLOR_BOLD_GREEN;
  }
  if (frac > 0.25f) {
    return COLOR_CYAN;
  }
  return COLOR_BLUE;
}

int main(void) {
  init_power_hold_early();

  stdio_init_all();
  sleep_ms(1500);

  printf("%s[BOOT]%s nfc_tune firmware\n", COLOR_GREEN, COLOR_RESET);
  printf("%s[INFO]%s I2C: SDA=GP%u SCL=GP%u @ %u Hz\n",
         COLOR_CYAN,
         COLOR_RESET,
         (unsigned)NFC_I2C_SDA_PIN,
         (unsigned)NFC_I2C_SCL_PIN,
         (unsigned)NFC_I2C_BAUDRATE_HZ);
  printf("%s[INFO]%s ST25 power enable: GP%d (delay=%u ms)\n",
         COLOR_CYAN,
         COLOR_RESET,
         NFC_ST25_VCC_EN_PIN,
         (unsigned)NFC_ST25_POWER_ON_DELAY_MS);
  printf("%s[INFO]%s ADC input: %u\n", COLOR_CYAN, COLOR_RESET, (unsigned)NFC_TUNE_ADC_INPUT);
  printf("%s[INFO]%s Samples/report: %u, report interval: %u ms\n",
         COLOR_CYAN,
         COLOR_RESET,
         (unsigned)NFC_TUNE_SAMPLE_COUNT,
         (unsigned)NFC_TUNE_REPORT_INTERVAL_MS);
  printf("%s[INFO]%s RF detect thresholds: ON=+%.1f mV OFF=+%.1f mV debounce=%u\n",
         COLOR_CYAN,
         COLOR_RESET,
         (double)NFC_TUNE_FIELD_ON_MV,
         (double)NFC_TUNE_FIELD_OFF_MV,
         (unsigned)NFC_TUNE_DEBOUNCE_COUNT);
  printf("%s[INFO]%s Baseline mode: %s\n",
         COLOR_CYAN,
         COLOR_RESET,
         NFC_TUNE_FREEZE_BASELINE ? "FROZEN_AFTER_STARTUP" : "TRACK_WHILE_RF_OFF");
  printf("%s[INFO]%s Graph: %ux%u bars, scale=0..%.1f mV, newest at right\n",
         COLOR_CYAN,
         COLOR_RESET,
         (unsigned)NFC_TUNE_GRAPH_WIDTH,
         (unsigned)NFC_TUNE_GRAPH_HEIGHT,
         (double)NFC_TUNE_GRAPH_MAX_MV);
  printf("%s[INFO]%s Graph redraw: ANSI home-mode + hidden cursor\n",
         COLOR_CYAN,
         COLOR_RESET);

  st25_power_on();
  sleep_ms(NFC_ST25_POWER_ON_DELAY_MS);

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
    printf("%s[ERR]%s ST25DV_RegisterBusIO failed: %ld\n", COLOR_BOLD_RED, COLOR_RESET, (long)ret);
    while (true) {
      sleep_ms(1000);
    }
  }

  ret = St25Dv_Drv.Init(&st);
  if (ret != NFCTAG_OK) {
    printf("%s[ERR]%s St25Dv_Drv.Init failed: %ld\n", COLOR_BOLD_RED, COLOR_RESET, (long)ret);
    while (true) {
      sleep_ms(1000);
    }
  }
  printf("%s[OK]%s ST25 init\n", COLOR_BOLD_GREEN, COLOR_RESET);

  uint8_t icref = 0;
  ret = St25Dv_Drv.ReadID(&st, &icref);
  if (ret == NFCTAG_OK) {
    printf("%s[INFO]%s ST25 ICREF: 0x%02X\n", COLOR_CYAN, COLOR_RESET, (unsigned)icref);
  } else {
    printf("%s[WARN]%s ST25 ReadID failed: %ld\n", COLOR_BOLD_YELLOW, COLOR_RESET, (long)ret);
  }

  ST25DV_MEM_SIZE mem = {0};
  ret = ST25DV_ReadMemSize(&st, &mem);
  if (ret == NFCTAG_OK) {
    const uint32_t blocks = (uint32_t)mem.Mem_Size + 1u;
    const uint32_t bytes_per_block = (uint32_t)mem.BlockSize + 1u;
    printf("%s[INFO]%s ST25 memory: blocks=%lu bytes_per_block=%lu total=%lu\n",
           COLOR_CYAN,
           COLOR_RESET,
           (unsigned long)blocks,
           (unsigned long)bytes_per_block,
           (unsigned long)(blocks * bytes_per_block));
  } else {
    printf("%s[WARN]%s ST25 ReadMemSize failed: %ld\n", COLOR_BOLD_YELLOW, COLOR_RESET, (long)ret);
  }

  if (!st25_enable_eh_mode(&st)) {
    printf("%s[ERR]%s EH setup failed (EH_EN must be 1). Stopping.\n", COLOR_BOLD_RED, COLOR_RESET);
    while (true) {
      sleep_ms(1000);
    }
  }

  const int adc_gpio = adc_input_to_gpio((uint)NFC_TUNE_ADC_INPUT);
  if (adc_gpio < 0) {
    printf("%s[ERR]%s Invalid NFC_TUNE_ADC_INPUT=%u (expected 0..3)\n",
           COLOR_BOLD_RED,
           COLOR_RESET,
           (unsigned)NFC_TUNE_ADC_INPUT);
    while (true) {
      sleep_ms(1000);
    }
  }

  adc_init();
  adc_set_temp_sensor_enabled(false);
  adc_gpio_init((uint)adc_gpio);
  adc_set_round_robin(0);
  adc_fifo_drain();
  adc_select_input((uint)NFC_TUNE_ADC_INPUT);
  sleep_ms(2);

  printf("%s[OK]%s ADC ready: GP%d (ADC%u)\n", COLOR_BOLD_GREEN, COLOR_RESET, adc_gpio, (unsigned)NFC_TUNE_ADC_INPUT);
  printf("%s[INFO]%s Baseline settle delay: %u ms\n", COLOR_CYAN, COLOR_RESET, (unsigned)NFC_TUNE_BASELINE_SETTLE_MS);
  sleep_ms(NFC_TUNE_BASELINE_SETTLE_MS);
  printf("%s[INFO]%s Baseline capture: keep RF away for ~%lu ms\n",
         COLOR_CYAN,
         COLOR_RESET,
         (unsigned long)((NFC_TUNE_BASELINE_SAMPLES * NFC_TUNE_SAMPLE_DELAY_US) / 1000u));
  const float baseline_raw0 = sample_adc_raw_average((uint)NFC_TUNE_ADC_INPUT, NFC_TUNE_BASELINE_SAMPLES);
  float baseline_v = adc_raw_to_v(baseline_raw0);
  printf("%s[OK]%s Baseline: raw=%.1f v=%.3fV\n", COLOR_BOLD_GREEN, COLOR_RESET, baseline_raw0, baseline_v);
  printf("%s[INFO]%s Graph/status update in place. Press Ctrl+C to stop.\n", COLOR_CYAN, COLOR_RESET);

  float ema_v = baseline_v;
  bool rf_on = false;
  uint32_t rise_hits = 0u;
  uint32_t fall_hits = 0u;
  uint32_t sessions = 0u;
  float session_peak_v = baseline_v;
  float session_peak_delta_mv = 0.0f;
  float last_peak_delta_mv = 0.0f;
  ST25DV_EH_CTRL eh_last = {0};
  bool eh_valid = false;
  uint32_t next_eh_recheck_ms = st25_get_tick();

  const float on_th_v = NFC_TUNE_FIELD_ON_MV / 1000.0f;
  const float off_th_v = NFC_TUNE_FIELD_OFF_MV / 1000.0f;
  float graph_hist[NFC_TUNE_GRAPH_WIDTH];
  uint32_t graph_head = 0u;
  for (uint32_t i = 0; i < (uint32_t)NFC_TUNE_GRAPH_WIDTH; i++) {
    graph_hist[i] = 0.0f;
  }
  const int32_t on_row = graph_row_for_mv(NFC_TUNE_FIELD_ON_MV, NFC_TUNE_GRAPH_MAX_MV, NFC_TUNE_GRAPH_HEIGHT);
  const int32_t off_row = graph_row_for_mv(NFC_TUNE_FIELD_OFF_MV, NFC_TUNE_GRAPH_MAX_MV, NFC_TUNE_GRAPH_HEIGHT);

  printf("\033[2J\033[H" TERM_HIDE_CURSOR);
  fflush(stdout);

  while (true) {
    printf("\033[H");

    const float raw_avg = sample_adc_raw_average((uint)NFC_TUNE_ADC_INPUT, NFC_TUNE_SAMPLE_COUNT);
    const uint16_t raw_u16 = (uint16_t)(raw_avg + 0.5f);
    const float v_avg = adc_raw_to_v(raw_avg);

    ema_v = (NFC_TUNE_EMA_ALPHA * v_avg) + ((1.0f - NFC_TUNE_EMA_ALPHA) * ema_v);

    if (!rf_on && !NFC_TUNE_FREEZE_BASELINE) {
      baseline_v = (NFC_TUNE_BASELINE_TRACK_ALPHA * ema_v) + ((1.0f - NFC_TUNE_BASELINE_TRACK_ALPHA) * baseline_v);
    }
    const float delta_v_raw = ema_v - baseline_v;
    float delta_v_pos = delta_v_raw;
    if (delta_v_pos < 0.0f) {
      delta_v_pos = 0.0f;
    }
    const float delta_mv_raw = delta_v_raw * 1000.0f;
    const float delta_mv_pos = delta_v_pos * 1000.0f;
    graph_hist[graph_head] = delta_mv_pos;
    graph_head = (graph_head + 1u) % (uint32_t)NFC_TUNE_GRAPH_WIDTH;

    if (!rf_on) {
      if (delta_v_pos >= on_th_v) {
        rise_hits++;
      } else {
        rise_hits = 0u;
      }
      if (rise_hits >= NFC_TUNE_DEBOUNCE_COUNT) {
        rf_on = true;
        rise_hits = 0u;
        fall_hits = 0u;
        sessions++;
        session_peak_v = ema_v;
        session_peak_delta_mv = delta_mv_pos;
      }
    } else {
      if (ema_v > session_peak_v) {
        session_peak_v = ema_v;
      }
      if (delta_mv_pos > session_peak_delta_mv) {
        session_peak_delta_mv = delta_mv_pos;
      }
      if (delta_v_pos <= off_th_v) {
        fall_hits++;
      } else {
        fall_hits = 0u;
      }
      if (fall_hits >= NFC_TUNE_DEBOUNCE_COUNT) {
        rf_on = false;
        fall_hits = 0u;
        rise_hits = 0u;
        last_peak_delta_mv = session_peak_delta_mv;
      }
    }

    ST25DV_EH_CTRL eh_now = {0};
    if (st25_read_eh_ctrl_quick(&st, &eh_now)) {
      eh_last = eh_now;
      eh_valid = true;
    }

    const uint32_t now_ms = st25_get_tick();
    const bool eh_needs_repair =
        eh_valid &&
        (eh_last.EH_EN_Mode == ST25DV_DISABLE ||
         (eh_last.Field_on == ST25DV_ENABLE && eh_last.EH_on == ST25DV_DISABLE));
    if (eh_needs_repair && tick_elapsed(now_ms, next_eh_recheck_ms)) {
      ST25DV_EH_CTRL eh_retry = {0};
      bool repaired = st25_enforce_eh_enabled(&st, false, &eh_retry);
      if (!repaired && eh_last.Field_on == ST25DV_ENABLE) {
        repaired = st25_rearm_eh_path(&st, false, &eh_retry);
      }
      if (repaired) {
        eh_last = eh_retry;
        eh_valid = true;
      }
      next_eh_recheck_ms = now_ms + (uint32_t)NFC_TUNE_EH_RECHECK_MS;
    }

    const bool sat = raw_u16 >= 4090u;
    const char* rf_color = rf_on ? COLOR_BOLD_GREEN : COLOR_BOLD_YELLOW;
    const char* sat_color = sat ? COLOR_BOLD_RED : COLOR_GREEN;
    const char* delta_color = rf_on ? COLOR_BOLD_GREEN : COLOR_YELLOW;

    const unsigned eh_en = eh_valid ? (unsigned)eh_last.EH_EN_Mode : 0u;
    const unsigned eh_on = eh_valid ? (unsigned)eh_last.EH_on : 0u;
    const unsigned field_on = eh_valid ? (unsigned)eh_last.Field_on : 0u;
    const unsigned vcc_on = eh_valid ? (unsigned)eh_last.VCC_on : 0u;
    const char* eh_tag = eh_valid ? "OK" : "NA";

    for (uint32_t row = 0u; row < (uint32_t)NFC_TUNE_GRAPH_HEIGHT; row++) {
      const float y_mv = NFC_TUNE_GRAPH_MAX_MV * (float)((uint32_t)NFC_TUNE_GRAPH_HEIGHT - row) /
                         (float)(uint32_t)NFC_TUNE_GRAPH_HEIGHT;
      printf("\r%s%6.1f%s |", COLOR_CYAN, (double)y_mv, COLOR_RESET);
      for (uint32_t col = 0u; col < (uint32_t)NFC_TUNE_GRAPH_WIDTH; col++) {
        const uint32_t src = (graph_head + col) % (uint32_t)NFC_TUNE_GRAPH_WIDTH;
        const uint32_t h = graph_bar_height(graph_hist[src], NFC_TUNE_GRAPH_MAX_MV, NFC_TUNE_GRAPH_HEIGHT);
        const bool filled = h >= ((uint32_t)NFC_TUNE_GRAPH_HEIGHT - row);
        if (filled) {
          const char* bar_color = graph_color_for_row(row, NFC_TUNE_GRAPH_HEIGHT);
          printf("%s#%s", bar_color, COLOR_RESET);
        } else if ((int32_t)row == on_row) {
          printf("%s-%s", COLOR_RED, COLOR_RESET);
        } else if ((int32_t)row == off_row) {
          printf("%s.%s", COLOR_YELLOW, COLOR_RESET);
        } else if ((row % 5u) == 0u) {
          printf("%s.%s", COLOR_DIM, COLOR_RESET);
        } else {
          printf(" ");
        }
      }
      printf("|\n");
    }

    printf("\r       +");
    for (uint32_t col = 0u; col < (uint32_t)NFC_TUNE_GRAPH_WIDTH; col++) {
      if ((col % 10u) == 9u) {
        printf("+");
      } else {
        printf("-");
      }
    }
    printf("+\n");

    const char* delta_sign_color = (delta_mv_raw >= 0.0f) ? delta_color : COLOR_RED;
    printf("\r%s[TUNE]%s RF=%s%s%s raw=%4u v=%.3fV ema=%.3fV base=%.3fV d=%s%+6.1fmV%s peak=%6.1fmV sess=%3lu | EH:%s en=%u on=%u f=%u vcc=%u | %s%s%s      ",
           COLOR_CYAN,
           COLOR_RESET,
           rf_color,
           rf_on ? "ON " : "OFF",
           COLOR_RESET,
           (unsigned)raw_u16,
           v_avg,
           ema_v,
           baseline_v,
           delta_sign_color,
           delta_mv_raw,
           COLOR_RESET,
           (double)(rf_on ? session_peak_delta_mv : last_peak_delta_mv),
           (unsigned long)sessions,
           eh_tag,
           eh_en,
           eh_on,
           field_on,
           vcc_on,
           sat_color,
           sat ? "SAT" : "OK ",
           COLOR_RESET);
    fflush(stdout);

    sleep_ms(NFC_TUNE_REPORT_INTERVAL_MS);
  }
}
