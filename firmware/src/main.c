#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"
#if NFC_HAS_CYW43
#include "pico/cyw43_arch.h"
#endif

#include "st25dv.h"

#ifndef NFC_HAS_CYW43
#define NFC_HAS_CYW43 0
#endif

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

#ifndef NFC_STATUS_LED_PIN
#define NFC_STATUS_LED_PIN 15
#endif

#ifndef NFC_POWER_LED_PIN
#define NFC_POWER_LED_PIN 25
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

#ifndef NFC_ENABLE_BOOT_RW_TIMING_TEST
#define NFC_ENABLE_BOOT_RW_TIMING_TEST 1
#endif

#ifndef NFC_ENABLE_STARTUP_DIAGNOSTICS
#define NFC_ENABLE_STARTUP_DIAGNOSTICS 1
#endif

#ifndef NFC_ENABLE_EH_TEST_MODE
#define NFC_ENABLE_EH_TEST_MODE 1
#endif

#ifndef NFC_I2C_OP_TIMEOUT_US
#define NFC_I2C_OP_TIMEOUT_US 20000
#endif

#ifndef NFC_ENABLE_WAKE_GPO_CONFIG
#define NFC_ENABLE_WAKE_GPO_CONFIG 1
#endif

#ifndef NFC_WAKE_GPO_SELFTEST_STRICT
#define NFC_WAKE_GPO_SELFTEST_STRICT 0
#endif

#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD_RED "\033[1;31m"
#define COLOR_BOLD_GREEN "\033[1;32m"
#define COLOR_BOLD_YELLOW "\033[1;33m"

static void log_vcolor(const char* color, const char* tag, const char* fmt, va_list args) {
  printf("%s[%s]%s ", color, tag, COLOR_RESET);
  vprintf(fmt, args);
}

static void log_color(const char* color, const char* tag, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_vcolor(color, tag, fmt, args);
  va_end(args);
}

static void log_info(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_vcolor(COLOR_CYAN, "INFO", fmt, args);
  va_end(args);
}

static void log_ok(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_vcolor(COLOR_BOLD_GREEN, "OK", fmt, args);
  va_end(args);
}

static void log_warn(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_vcolor(COLOR_BOLD_YELLOW, "WARN", fmt, args);
  va_end(args);
}

static void log_err(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_vcolor(COLOR_BOLD_RED, "ERR", fmt, args);
  va_end(args);
}

static void i2c_setup(void) {
  i2c_init(NFC_I2C_INSTANCE, NFC_I2C_BAUDRATE_HZ);
  gpio_set_function(NFC_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(NFC_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(NFC_I2C_SDA_PIN);
  gpio_pull_up(NFC_I2C_SCL_PIN);
}

static void st25_power_on(void) {
#if (NFC_ST25_VCC_EN_PIN >= 0)
  gpio_init(NFC_ST25_VCC_EN_PIN);
  gpio_set_dir(NFC_ST25_VCC_EN_PIN, GPIO_OUT);
  gpio_put(NFC_ST25_VCC_EN_PIN, 1);
#endif
}

static bool i2c_address_acks(uint8_t address_7bit) {
  const uint8_t probe_addr[2] = {0x00, 0x00};
  const int res = i2c_write_timeout_us(
      NFC_I2C_INSTANCE, address_7bit, probe_addr, (size_t)sizeof(probe_addr), false, NFC_I2C_OP_TIMEOUT_US);
  return res == (int)sizeof(probe_addr);
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

typedef struct {
  uint8_t version;
  uint8_t opcode;
  uint16_t duration_min;
  uint32_t unix_seconds;
  uint32_t nonce;
} InkiRequest;

static bool decode_inki_request(const uint8_t req16[16], InkiRequest* out) {
  if (!is_inki_request(req16)) {
    return false;
  }

  if (out != NULL) {
    out->version = req16[4];
    out->opcode = req16[5];
    out->duration_min = (uint16_t)req16[6] | ((uint16_t)req16[7] << 8);
    out->unix_seconds = (uint32_t)req16[8] | ((uint32_t)req16[9] << 8) |
                        ((uint32_t)req16[10] << 16) | ((uint32_t)req16[11] << 24);
    out->nonce = (uint32_t)req16[12] | ((uint32_t)req16[13] << 8) |
                 ((uint32_t)req16[14] << 16) | ((uint32_t)req16[15] << 24);
  }

  return true;
}

static bool validate_inki_request(const InkiRequest* req, const char** reason) {
  if (req->version != 1u) {
    if (reason != NULL) {
      *reason = "version";
    }
    return false;
  }
  if (req->opcode == 0u) {
    if (reason != NULL) {
      *reason = "opcode";
    }
    return false;
  }
  if (req->duration_min == 0u || req->duration_min > (24u * 60u)) {
    if (reason != NULL) {
      *reason = "duration";
    }
    return false;
  }
  if (req->unix_seconds == 0u) {
    if (reason != NULL) {
      *reason = "unix";
    }
    return false;
  }
  if (req->nonce == 0u) {
    if (reason != NULL) {
      *reason = "nonce";
    }
    return false;
  }
  return true;
}

static bool request_is_all_zero(const uint8_t req16[16]) {
  for (size_t i = 0; i < 16; i++) {
    if (req16[i] != 0) {
      return false;
    }
  }
  return true;
}

static void log_request_bytes(uint16_t addr, const uint8_t req16[16]) {
  printf("%s[INFO]%s Request@0x%04X: ", COLOR_CYAN, COLOR_RESET, addr);
  dump_hex(req16, 16);
  printf("\n");
}

static const char* st25_icref_name(uint8_t icref) {
  switch (icref) {
    case 0x24:
      return "ST25DV04 (legacy)";
    case 0x25:
      return "ST25DV04 CMOS (legacy)";
    case 0x26:
      return "ST25DV16/64 (legacy)";
    case 0x27:
      return "ST25DV16/64 CMOS (legacy)";
    case 0x50:
      return "ST25DV04KC";
    case 0x51:
      return "ST25DV16KC/ST25DV64KC";
    default:
      return "Unknown";
  }
}

static const char* st25_part_from_size(uint32_t total_bytes) {
  switch (total_bytes) {
    case 512:
      return "ST25DV04KC";
    case 2048:
      return "ST25DV16KC";
    case 8192:
      return "ST25DV64KC";
    default:
      return "Unknown capacity";
  }
}

static const char* st25_uid_product_code_name(uint8_t product_code) {
  switch (product_code) {
    case 0x50:
      return "ST25DV04KC-IE";
    case 0x51:
      return "ST25DV16KC/64KC-IE";
    case 0x52:
      return "ST25DV04KC-JF";
    case 0x53:
      return "ST25DV16KC/64KC-JF";
    default:
      return "Unknown product code";
  }
}

static const char* st25_eh_mode_name(ST25DV_EH_MODE_STATUS mode) {
  switch (mode) {
    case ST25DV_EH_ACTIVE_AFTER_BOOT:
      return "ACTIVE_AFTER_BOOT";
    case ST25DV_EH_ON_DEMAND:
      return "ON_DEMAND";
    default:
      return "UNKNOWN";
  }
}

static const char* st25_en_name(ST25DV_EN_STATUS status) {
  return (status == ST25DV_ENABLE) ? "ON" : "OFF";
}

static const char* st25_field_name(ST25DV_FIELD_STATUS status) {
  return (status == ST25DV_FIELD_ON) ? "ON" : "OFF";
}

static const char* st25_vcc_name(ST25DV_VCC_STATUS status) {
  return (status == ST25DV_VCC_ON) ? "ON" : "OFF";
}

static const char* st25_session_name(ST25DV_I2CSSO_STATUS status) {
  return (status == ST25DV_SESSION_OPEN) ? "OPEN" : "CLOSED";
}

static const char* st25_current_msg_name(ST25DV_CURRENT_MSG msg) {
  switch (msg) {
    case ST25DV_NO_MSG:
      return "NO_MSG";
    case ST25DV_HOST_MSG:
      return "HOST_MSG";
    case ST25DV_RF_MSG:
      return "RF_MSG";
    default:
      return "UNKNOWN";
  }
}

static void init_power_hold_early(void) {
#if (NFC_POWER_HOLD_PIN >= 0)
  gpio_init(NFC_POWER_HOLD_PIN);
  gpio_set_dir(NFC_POWER_HOLD_PIN, GPIO_OUT);
  gpio_put(NFC_POWER_HOLD_PIN, 1);
#endif
}

static uint32_t st25_it_pulse_us(ST25DV_PULSE_DURATION pulse) {
  switch (pulse) {
    case ST25DV_302_US:
      return 302u;
    case ST25DV_264_US:
      return 264u;
    case ST25DV_226_US:
      return 226u;
    case ST25DV_188_US:
      return 188u;
    case ST25DV_151_US:
      return 151u;
    case ST25DV_113_US:
      return 113u;
    case ST25DV_75_US:
      return 75u;
    case ST25DV_37_US:
      return 37u;
    default:
      return 0u;
  }
}

static int32_t st25_write_itpulse_retry(ST25DV_Object_t* st,
                                        ST25DV_PULSE_DURATION pulse,
                                        uint32_t timeout_ms,
                                        uint32_t retry_delay_ms,
                                        uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = ST25DV_WriteITPulse(st, pulse);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static int32_t st25_config_it_retry(ST25DV_Object_t* st,
                                    uint16_t it_conf,
                                    uint32_t timeout_ms,
                                    uint32_t retry_delay_ms,
                                    uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = St25Dv_Drv.ConfigIT(st, it_conf);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static int32_t st25_get_it_status_retry(ST25DV_Object_t* st,
                                        uint16_t* it_status,
                                        uint32_t timeout_ms,
                                        uint32_t retry_delay_ms,
                                        uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = St25Dv_Drv.GetITStatus(st, it_status);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static int32_t st25_read_eh_mode_retry(ST25DV_Object_t* st,
                                       ST25DV_EH_MODE_STATUS* eh_mode,
                                       uint32_t timeout_ms,
                                       uint32_t retry_delay_ms,
                                       uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = ST25DV_ReadEHMode(st, eh_mode);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static bool configure_wake_gpo(ST25DV_Object_t* st) {
#if NFC_ENABLE_WAKE_GPO_CONFIG
  bool ok = true;
  int32_t ret = NFCTAG_OK;
  const uint32_t cfg_timeout_ms = (uint32_t)ST25DV_WRITE_TIMEOUT + 50u;
  const uint32_t cfg_retry_delay_ms = 2u;
  uint32_t attempts = 0;

  ret = st25_write_itpulse_retry(st, ST25DV_302_US, cfg_timeout_ms, cfg_retry_delay_ms, &attempts);
  if (ret == NFCTAG_OK) {
    log_ok("Wake GPO pulse set: IT_TIME=0 (~302 us)\n");
    if (attempts > 1) {
      log_warn("Wake GPO pulse config retries: %lu\n", (unsigned long)attempts);
    }
  } else {
    log_err("Wake GPO pulse config failed after %lu attempt(s): %ld\n", (unsigned long)attempts, (long)ret);
    ok = false;
  }

  const uint16_t wake_conf = (uint16_t)(ST25DV_GPO_ENABLE_MASK | ST25DV_GPO_FIELDCHANGE_MASK | ST25DV_GPO_RFWRITE_MASK);
  ret = st25_config_it_retry(st, wake_conf, cfg_timeout_ms, cfg_retry_delay_ms, &attempts);
  if (ret == NFCTAG_OK) {
    log_ok("Wake GPO sources set: ENABLE + FIELD_CHANGE + RF_WRITE\n");
    if (attempts > 1) {
      log_warn("Wake GPO source config retries: %lu\n", (unsigned long)attempts);
    }
  } else {
    log_err("Wake GPO source config failed after %lu attempt(s): %ld\n", (unsigned long)attempts, (long)ret);
    ok = false;
  }

  uint16_t gpo_status = 0;
  ret = st25_get_it_status_retry(st, &gpo_status, cfg_timeout_ms, cfg_retry_delay_ms, &attempts);
  if (ret == NFCTAG_OK) {
    log_info("Wake GPO config readback: 0x%02X\n", (unsigned)(gpo_status & 0xFFu));
    if (attempts > 1) {
      log_warn("Wake GPO readback retries: %lu\n", (unsigned long)attempts);
    }
  } else {
    log_warn("Wake GPO readback failed after %lu attempt(s): %ld\n", (unsigned long)attempts, (long)ret);
    ok = false;
  }

  return ok;
#else
  (void)st;
  log_info("Wake GPO config disabled (NFC_ENABLE_WAKE_GPO_CONFIG=0)\n");
  return true;
#endif
}

static void append_reason(char* reasons, size_t reasons_size, const char* reason) {
  if (reasons_size == 0 || reason == NULL || reason[0] == '\0') {
    return;
  }

  const size_t used = strlen(reasons);
  if (used >= reasons_size - 1) {
    return;
  }

  const char* sep = (used == 0) ? "" : ", ";
  (void)snprintf(reasons + used, reasons_size - used, "%s%s", sep, reason);
}

static bool print_i2c_probe_summary(void) {
  const bool ack_53 = i2c_address_acks(0x53);
  const bool ack_57 = i2c_address_acks(0x57);

  log_info("I2C probe (7-bit): 0x53=%s 0x57=%s\n", ack_53 ? "ACK" : "NACK", ack_57 ? "ACK" : "NACK");
  if (ack_53 && ack_57) {
    log_ok("I2C address probe passed\n");
  } else {
    log_warn("I2C address probe incomplete\n");
  }
  return ack_53 && ack_57;
}

static bool print_dynamic_status(ST25DV_Object_t* st) {
  bool all_ok = true;
  int32_t ret = NFCTAG_OK;

  ST25DV_I2CSSO_STATUS session = ST25DV_SESSION_CLOSED;
  ret = ST25DV_ReadI2CSecuritySession_Dyn(st, &session);
  if (ret == NFCTAG_OK) {
    log_info("DYN I2C session: %s (security session)\n", st25_session_name(session));
  } else {
    log_err("DYN I2C session read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  ST25DV_EH_CTRL eh = {0};
  ret = ST25DV_ReadEHCtrl_Dyn(st, &eh);
  if (ret == NFCTAG_OK) {
    log_info("DYN EH: EH_EN=%s EH_ON=%s FIELD_ON=%s VCC_ON=%s (EH ctrl bits)\n",
             st25_en_name(eh.EH_EN_Mode),
             st25_en_name(eh.EH_on),
             st25_en_name(eh.Field_on),
             st25_en_name(eh.VCC_on));
  } else {
    log_err("DYN EH_CTRL read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  ST25DV_FIELD_STATUS rf_field = ST25DV_FIELD_OFF;
  ret = ST25DV_GetRFField_Dyn(st, &rf_field);
  if (ret == NFCTAG_OK) {
    log_info("DYN RF field: %s (carrier detect)\n", st25_field_name(rf_field));
  } else {
    log_err("DYN RF field read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  ST25DV_VCC_STATUS vcc = ST25DV_VCC_OFF;
  ret = ST25DV_GetVCC_Dyn(st, &vcc);
  if (ret == NFCTAG_OK) {
    log_info("DYN VCC seen by ST25: %s (tag VCC state)\n", st25_vcc_name(vcc));
  } else {
    log_err("DYN VCC status read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  ST25DV_RF_MNGT rf_mngt = {0};
  ret = ST25DV_ReadRFMngt_Dyn(st, &rf_mngt);
  if (ret == NFCTAG_OK) {
    log_info("DYN RF mngt: RF_DISABLE=%s RF_SLEEP=%s (RF interface ctrl)\n",
             st25_en_name(rf_mngt.RfDisable),
             st25_en_name(rf_mngt.RfSleep));
  } else {
    log_err("DYN RF management read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  uint8_t it_status = 0;
  ret = ST25DV_ReadITSTStatus_Dyn(st, &it_status);
  if (ret == NFCTAG_OK) {
    log_info("DYN IT status: 0x%02X (latched event bits)\n", it_status);
  } else {
    log_err("DYN IT status read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  uint8_t gpo_dyn = 0;
  ret = ST25DV_ReadGPO_Dyn(st, &gpo_dyn);
  if (ret == NFCTAG_OK) {
    log_info("DYN GPO: 0x%02X (GPO dynamic reg)\n", gpo_dyn);
  } else {
    log_err("DYN GPO read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  ST25DV_PULSE_DURATION it_pulse = ST25DV_188_US;
  ret = ST25DV_ReadITPulse(st, &it_pulse);
  if (ret == NFCTAG_OK) {
    log_info("DYN GPO IT pulse: IT_TIME=%u (~%lu us)\n",
             (unsigned)it_pulse,
             (unsigned long)st25_it_pulse_us(it_pulse));
  } else {
    log_err("DYN GPO IT pulse read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  ST25DV_MB_CTRL_DYN_STATUS mb = {0};
  ret = ST25DV_ReadMBCtrl_Dyn(st, &mb);
  if (ret == NFCTAG_OK) {
    log_info("DYN mailbox: MBEN=%u HOSTPUT=%u RFPUT=%u HOSTMISS=%u RFMISS=%u CUR=%s (FTM status)\n",
             (unsigned)mb.MbEnable,
             (unsigned)mb.HostPutMsg,
             (unsigned)mb.RfPutMsg,
             (unsigned)mb.HostMissMsg,
             (unsigned)mb.RFMissMsg,
             st25_current_msg_name(mb.CurrentMsg));
  } else {
    log_err("DYN mailbox ctrl read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  uint8_t mb_len = 0;
  ret = ST25DV_ReadMBLength_Dyn(st, &mb_len);
  if (ret == NFCTAG_OK) {
    log_info("DYN mailbox length: %u (FTM bytes)\n", (unsigned)mb_len);
  } else {
    log_err("DYN mailbox length read failed: %ld\n", (long)ret);
    all_ok = false;
  }

  if (all_ok) {
    log_ok("Dynamic status diagnostics passed\n");
  }
  return all_ok;
}

static bool configure_eh_test_mode(ST25DV_Object_t* st) {
  bool ok = true;
  int32_t ret = NFCTAG_OK;
  const uint32_t eh_verify_timeout_ms = (uint32_t)ST25DV_WRITE_TIMEOUT + 50u;
  const uint32_t eh_verify_retry_delay_ms = 2u;
  uint32_t eh_verify_attempts = 0;

  ST25DV_EH_MODE_STATUS eh_mode = ST25DV_EH_ACTIVE_AFTER_BOOT;
  ret = ST25DV_ReadEHMode(st, &eh_mode);
  if (ret == NFCTAG_OK) {
    log_info("EH mode (before): %s\n", st25_eh_mode_name(eh_mode));
  } else {
    log_warn("EH mode read failed: %ld\n", (long)ret);
    ok = false;
  }

  log_info("EH mode write request: ACTIVE_AFTER_BOOT\n");
  ret = ST25DV_WriteEHMode(st, ST25DV_EH_ACTIVE_AFTER_BOOT);
  if (ret == NFCTAG_OK) {
    log_ok("EH mode write: ACTIVE_AFTER_BOOT\n");
  } else {
    log_err("EH mode write failed: %ld\n", (long)ret);
    ok = false;
  }

  eh_mode = ST25DV_EH_ACTIVE_AFTER_BOOT;
  ret = st25_read_eh_mode_retry(st, &eh_mode, eh_verify_timeout_ms, eh_verify_retry_delay_ms, &eh_verify_attempts);
  if (ret == NFCTAG_OK) {
    log_info("EH mode (after): %s\n", st25_eh_mode_name(eh_mode));
    if (eh_verify_attempts > 1) {
      log_warn("EH mode verify retries: %lu\n", (unsigned long)eh_verify_attempts);
    }
    if (eh_mode != ST25DV_EH_ACTIVE_AFTER_BOOT) {
      log_warn("EH mode verify mismatch\n");
      ok = false;
    }
  } else {
    log_warn("EH mode verify read failed: %ld\n", (long)ret);
    ok = false;
  }

#if NFC_ENABLE_EH_TEST_MODE
  ret = ST25DV_SetEHENMode_Dyn(st);
  if (ret == NFCTAG_OK) {
    log_ok("EH test mode: dynamic EH_EN set ON\n");
  } else {
    log_err("EH test mode enable failed: %ld\n", (long)ret);
    ok = false;
  }
#else
  log_info("EH test mode disabled (NFC_ENABLE_EH_TEST_MODE=0)\n");
#endif

  ST25DV_EH_CTRL eh = {0};
  ret = ST25DV_ReadEHCtrl_Dyn(st, &eh);
  if (ret == NFCTAG_OK) {
    log_info("EH state: EH_EN=%s EH_ON=%s FIELD_ON=%s VCC_ON=%s\n",
             st25_en_name(eh.EH_EN_Mode),
             st25_en_name(eh.EH_on),
             st25_en_name(eh.Field_on),
             st25_en_name(eh.VCC_on));
  } else {
    log_warn("EH state read failed: %ld\n", (long)ret);
    ok = false;
  }

  return ok;
}

static bool compute_request_region(uint32_t total_bytes,
                                   uint32_t bytes_per_block,
                                   uint16_t* req_addr,
                                   uint32_t* region_bytes) {
  if (total_bytes < 16) {
    return false;
  }

  const uint32_t blocks_needed = (16u + bytes_per_block - 1u) / bytes_per_block;
  const uint32_t bytes = blocks_needed * bytes_per_block;
  if (bytes > total_bytes) {
    return false;
  }

  *region_bytes = bytes;
  *req_addr = (uint16_t)(total_bytes - bytes);
  return true;
}

static void fill_boot_rw_test_pattern(uint8_t* pattern, uint32_t region_bytes) {
  for (uint32_t i = 0; i < region_bytes; i++) {
    pattern[i] = (uint8_t)(0xA5u ^ (uint8_t)(i * 0x3Du));
  }
}

static bool matches_boot_rw_test_pattern16(const uint8_t req16[16]) {
  uint8_t pattern16[16] = {0};
  fill_boot_rw_test_pattern(pattern16, sizeof(pattern16));
  return (memcmp(req16, pattern16, sizeof(pattern16)) == 0);
}

static int32_t st25_read_data_retry(ST25DV_Object_t* st,
                                    uint8_t* data,
                                    uint16_t addr,
                                    uint16_t len,
                                    uint32_t timeout_ms,
                                    uint32_t retry_delay_ms,
                                    uint32_t* elapsed_ms,
                                    uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = St25Dv_Drv.ReadData(st, data, addr, len);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (elapsed_ms != NULL) {
    *elapsed_ms = st25_get_tick() - t0;
  }
  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static int32_t st25_write_data_retry(ST25DV_Object_t* st,
                                     const uint8_t* data,
                                     uint16_t addr,
                                     uint16_t len,
                                     uint32_t timeout_ms,
                                     uint32_t retry_delay_ms,
                                     uint32_t* elapsed_ms,
                                     uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = St25Dv_Drv.WriteData(st, data, addr, len);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (elapsed_ms != NULL) {
    *elapsed_ms = st25_get_tick() - t0;
  }
  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static int32_t st25_get_rf_field_retry(ST25DV_Object_t* st,
                                       ST25DV_FIELD_STATUS* field,
                                       uint32_t timeout_ms,
                                       uint32_t retry_delay_ms,
                                       uint32_t* attempts) {
  const uint32_t t0 = st25_get_tick();
  uint32_t tries = 0;
  int32_t ret = NFCTAG_ERROR;

  do {
    tries++;
    ret = ST25DV_GetRFField_Dyn(st, field);
    if (ret == NFCTAG_OK) {
      break;
    }
    sleep_ms(retry_delay_ms);
  } while ((st25_get_tick() - t0) < timeout_ms);

  if (attempts != NULL) {
    *attempts = tries;
  }
  return ret;
}

static bool run_boot_rw_timing_test(ST25DV_Object_t* st,
                                    uint16_t req_addr,
                                    uint32_t region_bytes,
                                    const uint8_t req16[16],
                                    bool* executed) {
  *executed = false;

#if !NFC_ENABLE_BOOT_RW_TIMING_TEST
  (void)st;
  (void)req_addr;
  (void)region_bytes;
  (void)req16;
  log_info("Boot R/W timing test disabled\n");
  return true;
#else
  if (region_bytes == 0 || region_bytes > ST25DV_MAX_WRITE_BYTE) {
    log_warn("Boot R/W timing skipped: invalid region size %lu\n", (unsigned long)region_bytes);
    return false;
  }

  if (is_inki_request(req16)) {
    log_warn("Boot R/W timing skipped: live INKI request present\n");
    return true;
  }

  *executed = true;

  const uint32_t rw_retry_timeout_ms = (uint32_t)ST25DV_WRITE_TIMEOUT + 200u;
  const uint32_t rw_retry_delay_ms = 2u;

  uint8_t backup[ST25DV_MAX_WRITE_BYTE] = {0};
  uint8_t pattern[ST25DV_MAX_WRITE_BYTE] = {0};
  uint8_t verify[ST25DV_MAX_WRITE_BYTE] = {0};
  uint8_t restored[ST25DV_MAX_WRITE_BYTE] = {0};

  int32_t ret = St25Dv_Drv.ReadData(st, backup, req_addr, (uint16_t)region_bytes);
  if (ret != NFCTAG_OK) {
    log_err("Boot R/W timing backup read failed: %ld\n", (long)ret);
    return false;
  }

  fill_boot_rw_test_pattern(pattern, region_bytes);

  uint32_t t_write_ms = 0;
  uint32_t write_attempts = 0;
  ret = st25_write_data_retry(st,
                              pattern,
                              req_addr,
                              (uint16_t)region_bytes,
                              rw_retry_timeout_ms,
                              rw_retry_delay_ms,
                              &t_write_ms,
                              &write_attempts);
  if (ret != NFCTAG_OK) {
    (void)st25_write_data_retry(st,
                                backup,
                                req_addr,
                                (uint16_t)region_bytes,
                                rw_retry_timeout_ms,
                                rw_retry_delay_ms,
                                NULL,
                                NULL);
    log_err("Boot R/W timing pattern write failed after %lu attempt(s): %ld\n",
            (unsigned long)write_attempts,
            (long)ret);
    return false;
  }

  uint32_t t_verify_ms = 0;
  uint32_t verify_attempts = 0;
  ret = st25_read_data_retry(st,
                             verify,
                             req_addr,
                             (uint16_t)region_bytes,
                             rw_retry_timeout_ms,
                             rw_retry_delay_ms,
                             &t_verify_ms,
                             &verify_attempts);
  if (ret != NFCTAG_OK) {
    (void)st25_write_data_retry(st,
                                backup,
                                req_addr,
                                (uint16_t)region_bytes,
                                rw_retry_timeout_ms,
                                rw_retry_delay_ms,
                                NULL,
                                NULL);
    log_err("Boot R/W timing verify read failed after %lu attempt(s): %ld\n",
            (unsigned long)verify_attempts,
            (long)ret);
    return false;
  }

  bool pattern_ok = (memcmp(pattern, verify, region_bytes) == 0);

  uint32_t t_restore_ms = 0;
  uint32_t restore_attempts = 0;
  ret = st25_write_data_retry(st,
                              backup,
                              req_addr,
                              (uint16_t)region_bytes,
                              rw_retry_timeout_ms,
                              rw_retry_delay_ms,
                              &t_restore_ms,
                              &restore_attempts);
  if (ret != NFCTAG_OK) {
    log_err("Boot R/W timing restore write failed after %lu attempt(s): %ld\n",
            (unsigned long)restore_attempts,
            (long)ret);
    return false;
  }

  uint32_t restore_verify_attempts = 0;
  ret = st25_read_data_retry(st,
                             restored,
                             req_addr,
                             (uint16_t)region_bytes,
                             rw_retry_timeout_ms,
                             rw_retry_delay_ms,
                             NULL,
                             &restore_verify_attempts);
  if (ret != NFCTAG_OK) {
    log_err("Boot R/W timing restore verify read failed after %lu attempt(s): %ld\n",
            (unsigned long)restore_verify_attempts,
            (long)ret);
    return false;
  }

  const bool restore_ok = (memcmp(backup, restored, region_bytes) == 0);

  if (pattern_ok && restore_ok) {
    log_ok("Boot R/W timing passed: write=%lums verify=%lums restore=%lums (%lu bytes @ 0x%04X)\n",
           (unsigned long)t_write_ms,
           (unsigned long)t_verify_ms,
           (unsigned long)t_restore_ms,
           (unsigned long)region_bytes,
           req_addr);
    if (write_attempts > 1 || verify_attempts > 1 || restore_attempts > 1 || restore_verify_attempts > 1) {
      log_warn("Boot R/W timing retries: write=%lu verify=%lu restore=%lu restore_verify=%lu\n",
               (unsigned long)write_attempts,
               (unsigned long)verify_attempts,
               (unsigned long)restore_attempts,
               (unsigned long)restore_verify_attempts);
    }
    return true;
  }

  log_err("Boot R/W timing failed: pattern_ok=%u restore_ok=%u\n",
          pattern_ok ? 1u : 0u,
          restore_ok ? 1u : 0u);
  return false;
#endif
}

typedef struct {
  ST25DV_Object_t st;
  uint32_t total_bytes;
  uint32_t bytes_per_block;
  uint16_t req_addr;
  uint32_t region_bytes;
  bool region_ok;
} HarnessState;

typedef struct {
  bool i2c_probe_ok;
  bool init_ok;
  bool wake_cfg_ok;
  bool id_ok;
  bool mem_ok;
  bool dyn_ok;
  bool req_read_ok;
  bool rw_test_ok;
  bool rw_test_executed;
  uint8_t req_boot[16];
} StartupDiag;

static void startup_diag_reset(StartupDiag* diag) {
  memset(diag, 0, sizeof(*diag));
  diag->wake_cfg_ok = true;
  diag->dyn_ok = true;
  diag->rw_test_ok = true;
}

static void boot_log_banner(void) {
  log_color(COLOR_GREEN, "BOOT", "NFC harness (RP2040/Pico) boot\n");
  log_info("I2C: SDA=GP%u SCL=GP%u @ %u Hz\n",
           NFC_I2C_SDA_PIN,
           NFC_I2C_SCL_PIN,
           (unsigned)NFC_I2C_BAUDRATE_HZ);
#if (NFC_POWER_HOLD_PIN >= 0)
  log_info("Power latch: GP%u -> HIGH (early)\n", NFC_POWER_HOLD_PIN);
#else
  log_warn("Power latch: disabled\n");
#endif
}

static void init_power_led(void) {
#if NFC_HAS_CYW43
  const int ret = cyw43_arch_init();
  if (ret != 0) {
    log_warn("Power LED: CYW43 init failed: %d\n", ret);
    return;
  }
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
  log_info("Power LED: CYW43 GPIO%u -> ON\n", (unsigned)CYW43_WL_GPIO_LED_PIN);
#elif (NFC_POWER_LED_PIN >= 0)
  gpio_init(NFC_POWER_LED_PIN);
  gpio_set_dir(NFC_POWER_LED_PIN, GPIO_OUT);
  gpio_put(NFC_POWER_LED_PIN, 1);
  log_info("Power LED: GP%u -> ON\n", NFC_POWER_LED_PIN);
#else
  log_info("Power LED: disabled\n");
#endif
}

static void init_led_and_st25_power(void) {
  init_power_led();

  gpio_init(NFC_STATUS_LED_PIN);
  gpio_set_dir(NFC_STATUS_LED_PIN, GPIO_OUT);
  gpio_put(NFC_STATUS_LED_PIN, 0);

#if (NFC_ST25_VCC_EN_PIN >= 0)
  log_info("ST25 power: GP%u -> HIGH (wait %u ms)\n",
           NFC_ST25_VCC_EN_PIN,
           (unsigned)NFC_ST25_POWER_ON_DELAY_MS);
#else
  log_warn("ST25 power: GPIO control disabled\n");
#endif
  st25_power_on();
  sleep_ms(NFC_ST25_POWER_ON_DELAY_MS);
}

static void fatal_blink_forever(void) {
  while (true) {
    gpio_xor_mask(1u << NFC_STATUS_LED_PIN);
    sleep_ms(250);
  }
}

static bool st25_register_bus(HarnessState* state) {
  ST25DV_IO_t io = {
      .Init = st25_bus_init,
      .DeInit = st25_bus_deinit,
      .IsReady = st25_is_ready,
      .Write = st25_write,
      .Read = st25_read,
      .GetTick = st25_get_tick,
  };

  const int32_t ret = ST25DV_RegisterBusIO(&state->st, &io);
  if (ret != NFCTAG_OK) {
    log_err("ST25DV_RegisterBusIO failed: %ld\n", (long)ret);
    return false;
  }
  return true;
}

static void run_identity_and_memory_bringup(HarnessState* state, StartupDiag* diag) {
  int32_t ret = St25Dv_Drv.Init(&state->st);
  if (ret == NFCTAG_OK) {
    diag->init_ok = true;
    log_ok("St25Dv_Drv.Init OK\n");
  } else {
    log_err("St25Dv_Drv.Init failed: %ld\n", (long)ret);
  }

  uint8_t icref = 0;
  ret = St25Dv_Drv.ReadID(&state->st, &icref);
  if (ret == NFCTAG_OK) {
    diag->id_ok = true;
    log_info("ICREF: 0x%02X (%s)\n", icref, st25_icref_name(icref));
  } else {
    log_err("St25Dv_Drv.ReadID failed: %ld\n", (long)ret);
  }

  ST25DV_MEM_SIZE mem = {0};
  ret = ST25DV_ReadMemSize(&state->st, &mem);
  if (ret == NFCTAG_OK) {
    diag->mem_ok = true;
    const uint32_t num_blocks = (uint32_t)mem.Mem_Size + 1;
    state->bytes_per_block = (uint32_t)mem.BlockSize + 1;
    state->total_bytes = num_blocks * state->bytes_per_block;

    log_info("Memory: blocks=%lu bytes_per_block=%lu total=%lu bytes\n",
             (unsigned long)num_blocks,
             (unsigned long)state->bytes_per_block,
             (unsigned long)state->total_bytes);
    log_info("Detected part by capacity: %s\n", st25_part_from_size(state->total_bytes));
  } else {
    log_err("ReadMemSize failed: %ld\n", (long)ret);
  }

  uint8_t icrev = 0;
  ret = ST25DV_ReadICRev(&state->st, &icrev);
  if (ret == NFCTAG_OK) {
    log_info("ICREV: 0x%02X\n", icrev);
  } else {
    log_err("ReadICRev failed: %ld\n", (long)ret);
  }

  ST25DV_UID uid = {0};
  ret = ST25DV_ReadUID(&state->st, &uid);
  if (ret == NFCTAG_OK) {
    const uint8_t uid_product_code = (uint8_t)((uid.MsbUid >> 8) & 0xFF);
    log_info("UID: %08lX%08lX\n", (unsigned long)uid.MsbUid, (unsigned long)uid.LsbUid);
    log_info("UID product code: 0x%02X (%s)\n", uid_product_code, st25_uid_product_code_name(uid_product_code));
  } else {
    log_err("ReadUID failed: %ld\n", (long)ret);
  }

  if (diag->id_ok && diag->mem_ok) {
    log_ok("Diag: I2C link OK (ID + memory map reads succeeded)\n");
  }
  if (!diag->init_ok && diag->id_ok && diag->mem_ok) {
    log_warn("Diag: ST driver init failed but core I2C diagnostics succeeded\n");
  }
  if (diag->init_ok) {
    diag->wake_cfg_ok = configure_wake_gpo(&state->st);
    (void)configure_eh_test_mode(&state->st);
  }
  log_info("Expected ST25DV addresses (7-bit): 0x53 (user/dynamic), 0x57 (system)\n");

  state->region_ok = false;
  if (diag->mem_ok) {
    state->region_ok =
        compute_request_region(state->total_bytes, state->bytes_per_block, &state->req_addr, &state->region_bytes);
  }
  if (!state->region_ok) {
    log_warn("Request region unavailable\n");
  }
}

static void read_boot_request_snapshot(HarnessState* state, StartupDiag* diag) {
  if (!state->region_ok) {
    return;
  }

  const int32_t ready = st25_is_ready(ST25DV_ADDR_DATA_I2C, 3);
  if (ready != NFCTAG_OK) {
    log_err("ST25DV not ready for boot request read: %ld\n", (long)ready);
    return;
  }

  const int32_t ret = St25Dv_Drv.ReadData(&state->st, diag->req_boot, state->req_addr, (uint16_t)sizeof(diag->req_boot));
  if (ret == NFCTAG_OK) {
    diag->req_read_ok = true;
    log_request_bytes(state->req_addr, diag->req_boot);
    if (!is_inki_request(diag->req_boot) && matches_boot_rw_test_pattern16(diag->req_boot)) {
      log_warn("Request slot currently contains the boot-test pattern from a previous run\n");
    }
  } else {
    log_err("Boot request read failed: %ld\n", (long)ret);
  }
}

static void print_selftest_summary(const HarnessState* state, const StartupDiag* diag) {
  char reasons[192] = {0};
  if (!diag->i2c_probe_ok) {
    append_reason(reasons, sizeof(reasons), "i2c_probe");
  }
  if (!diag->init_ok) {
    append_reason(reasons, sizeof(reasons), "st_init");
  }
  if (!diag->id_ok) {
    append_reason(reasons, sizeof(reasons), "read_id");
  }
  if (!diag->mem_ok) {
    append_reason(reasons, sizeof(reasons), "read_mem");
  }
#if NFC_WAKE_GPO_SELFTEST_STRICT
  if (!diag->wake_cfg_ok) {
    append_reason(reasons, sizeof(reasons), "wake_gpo");
  }
#else
  if (!diag->wake_cfg_ok) {
    log_warn("SELFTEST note: wake_gpo boot config incomplete (strict mode off)\n");
  }
#endif
  if (!diag->dyn_ok) {
    append_reason(reasons, sizeof(reasons), "dyn_status");
  }
  if (state->region_ok && !diag->req_read_ok) {
    append_reason(reasons, sizeof(reasons), "read_request");
  }
  if (diag->rw_test_executed && !diag->rw_test_ok) {
    append_reason(reasons, sizeof(reasons), "rw_timing");
  }

  if (reasons[0] == '\0') {
    log_ok("SELFTEST: PASS\n");
  } else {
    log_err("SELFTEST: FAIL (%s)\n", reasons);
  }
}

static void run_optional_startup_diagnostics(HarnessState* state, StartupDiag* diag) {
#if NFC_ENABLE_STARTUP_DIAGNOSTICS
  diag->i2c_probe_ok = print_i2c_probe_summary();
  diag->dyn_ok = print_dynamic_status(&state->st);

  read_boot_request_snapshot(state, diag);
  if (state->region_ok && diag->req_read_ok) {
    diag->rw_test_ok =
        run_boot_rw_timing_test(&state->st, state->req_addr, state->region_bytes, diag->req_boot, &diag->rw_test_executed);
  }

  print_selftest_summary(state, diag);
#else
  (void)state;
  (void)diag;
  log_info("Startup diagnostics disabled (NFC_ENABLE_STARTUP_DIAGNOSTICS=0)\n");
#endif
}

static void poll_request_loop(HarnessState* state) {
  const uint32_t rf_poll_ms = 120u;
  const uint32_t rf_field_timeout_ms = 12u;
  const uint32_t rf_field_retry_delay_ms = 2u;
  const uint32_t req_read_timeout_ms = 40u;
  const uint32_t req_read_retry_delay_ms = 2u;
  const uint32_t req_clear_timeout_ms = (uint32_t)ST25DV_WRITE_TIMEOUT + 200u;
  const uint32_t req_clear_retry_delay_ms = 2u;

  bool have_rf_state = false;
  bool rf_on = false;
  uint32_t rf_field_fail_streak = 0;
  bool req_read_error = false;
  bool have_last_req = false;
  bool pending_clear = false;
  uint8_t last_req[16] = {0};

  while (true) {
    if (!state->region_ok) {
      log_warn("Request region not available\n");
      gpio_put(NFC_STATUS_LED_PIN, 0);
      sleep_ms(1000);
      continue;
    }

    ST25DV_FIELD_STATUS field = ST25DV_FIELD_OFF;
    uint32_t rf_field_attempts = 0;
    int32_t ret = st25_get_rf_field_retry(
        &state->st, &field, rf_field_timeout_ms, rf_field_retry_delay_ms, &rf_field_attempts);
    if (ret != NFCTAG_OK) {
      rf_field_fail_streak++;
      if (rf_field_fail_streak == 1) {
        log_warn("RF field read transient failure after %lu attempt(s): %ld\n",
                 (unsigned long)rf_field_attempts,
                 (long)ret);
      } else if (rf_field_fail_streak == 5) {
        log_err("RF field read still failing (5 consecutive polls)\n");
      }
      gpio_put(NFC_STATUS_LED_PIN, 0);
      sleep_ms(rf_poll_ms);
      continue;
    }
    if (rf_field_fail_streak > 0) {
      log_ok("RF field read recovered after %lu failed poll(s)\n", (unsigned long)rf_field_fail_streak);
      rf_field_fail_streak = 0;
    }

    const bool rf_now_on = (field == ST25DV_FIELD_ON);
    if (!have_rf_state || rf_now_on != rf_on) {
      have_rf_state = true;
      rf_on = rf_now_on;
      log_info("RF field: %s\n", rf_on ? "ON" : "OFF");
      if (rf_on) {
        // Start each RF session with a fresh request-change baseline.
        have_last_req = false;
      }
    }

    gpio_put(NFC_STATUS_LED_PIN, rf_on ? 1 : 0);
    if (!rf_on) {
      if (pending_clear) {
        uint8_t zeros[ST25DV_MAX_WRITE_BYTE] = {0};
        uint32_t clear_elapsed_ms = 0;
        uint32_t clear_attempts = 0;
        ret = st25_write_data_retry(&state->st,
                                    zeros,
                                    state->req_addr,
                                    (uint16_t)state->region_bytes,
                                    req_clear_timeout_ms,
                                    req_clear_retry_delay_ms,
                                    &clear_elapsed_ms,
                                    &clear_attempts);
        if (ret == NFCTAG_OK) {
          log_ok("Cleared request after RF OFF (%lu bytes @ 0x%04X, attempts=%lu, %lums)\n",
                 (unsigned long)state->region_bytes,
                 state->req_addr,
                 (unsigned long)clear_attempts,
                 (unsigned long)clear_elapsed_ms);
          pending_clear = false;
          memset(last_req, 0, sizeof(last_req));
          have_last_req = false;
        } else {
          log_err("Deferred clear failed after %lu attempt(s): %ld\n", (unsigned long)clear_attempts, (long)ret);
        }
      }
      sleep_ms(rf_poll_ms);
      continue;
    }

    uint8_t req[16] = {0};
    uint32_t read_attempts = 0;
    ret = st25_read_data_retry(&state->st,
                               req,
                               state->req_addr,
                               (uint16_t)sizeof(req),
                               req_read_timeout_ms,
                               req_read_retry_delay_ms,
                               NULL,
                               &read_attempts);
    if (ret != NFCTAG_OK) {
      if (!req_read_error) {
        log_err("RF request read failed after %lu attempt(s): %ld\n", (unsigned long)read_attempts, (long)ret);
        req_read_error = true;
      }
      sleep_ms(rf_poll_ms);
      continue;
    }
    if (req_read_error) {
      log_ok("RF request read recovered\n");
      req_read_error = false;
    }

    if (have_last_req && memcmp(req, last_req, sizeof(req)) == 0) {
      sleep_ms(rf_poll_ms);
      continue;
    }

    memcpy(last_req, req, sizeof(req));
    have_last_req = true;

    log_request_bytes(state->req_addr, req);

    if (request_is_all_zero(req)) {
      log_info("RF request slot is empty\n");
    } else if (is_inki_request(req)) {
      InkiRequest parsed = {0};
      (void)decode_inki_request(req, &parsed);
      const char* invalid_reason = NULL;
      if (validate_inki_request(&parsed, &invalid_reason)) {
        log_ok("INKI request: version=%u opcode=0x%02X duration_min=%u unix=%lu nonce=0x%08lX\n",
               parsed.version,
               parsed.opcode,
               parsed.duration_min,
               (unsigned long)parsed.unix_seconds,
               (unsigned long)parsed.nonce);
        pending_clear = true;
        log_info("Queued clear after RF field OFF\n");
      } else {
        log_warn("INKI request ignored: invalid/incomplete (%s)\n", invalid_reason != NULL ? invalid_reason : "format");
      }
    } else {
      log_warn("RF request ignored: missing INKI magic\n");
    }

    sleep_ms(rf_poll_ms);
  }
}

int main(void) {
  init_power_hold_early();

  stdio_init_all();
  sleep_ms(1500);

  boot_log_banner();
  init_led_and_st25_power();

  HarnessState state = {0};
  StartupDiag diag;
  startup_diag_reset(&diag);

  if (!st25_register_bus(&state)) {
    fatal_blink_forever();
  }

  run_identity_and_memory_bringup(&state, &diag);
  run_optional_startup_diagnostics(&state, &diag);
  poll_request_loop(&state);
}
