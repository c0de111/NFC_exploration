cmake_minimum_required(VERSION 3.13)

# Default to Pico W; override via `-DPICO_BOARD=...` or `PICO_BOARD=...`.
if(NOT DEFINED PICO_BOARD)
  set(PICO_BOARD pico_w CACHE STRING "Pico board type (e.g. pico, pico_w)")
endif()

# Default: avoid fetching/building picotool (SWD users don't need it); UF2 via picotool is disabled.
set(PICO_NO_PICOTOOL ON CACHE BOOL "Disable picotool (skips picotool-based UF2/signing)")
set(PICO_NO_UF2 ON CACHE BOOL "Disable UF2 generation via picotool")

include(${CMAKE_CURRENT_LIST_DIR}/cmake/pico_sdk_import.cmake)

project(nfc_exploration C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(nfc_harness
  src/main.c
  third_party/st25dv/st25dv.c
  third_party/st25dv/st25dv_reg.c
)

add_executable(nfc_tune
  src/tune_main.c
  third_party/st25dv/st25dv.c
  third_party/st25dv/st25dv_reg.c
)

set(NFC_I2C_BAUDRATE_HZ 100000 CACHE STRING "I2C baudrate in Hz")
set(NFC_I2C_SDA_PIN 20 CACHE STRING "I2C SDA GPIO (RP2040 pin number)")
set(NFC_I2C_SCL_PIN 21 CACHE STRING "I2C SCL GPIO (RP2040 pin number)")
set(NFC_STATUS_LED_PIN 15 CACHE STRING "Status LED GPIO (RP2040 pin number)")
set(NFC_POWER_LED_PIN 25 CACHE STRING "Power LED GPIO (set -1 to disable)")
set(NFC_POWER_HOLD_PIN 28 CACHE STRING "GPIO used to latch board power (set -1 to disable)")
set(NFC_ST25_VCC_EN_PIN 18 CACHE STRING "GPIO used to enable ST25 VCC (set -1 to disable)")
set(NFC_ST25_POWER_ON_DELAY_MS 10 CACHE STRING "Delay after enabling ST25 VCC before I2C access")
set(NFC_I2C_OP_TIMEOUT_US 20000 CACHE STRING "I2C op timeout in microseconds for read/write/probe transactions")
set(NFC_ENABLE_WAKE_GPO_CONFIG 1 CACHE STRING "Configure ST25 GPO for NFC wake events at boot")
set(NFC_WAKE_GPO_SELFTEST_STRICT 0 CACHE STRING "Fail SELFTEST if wake GPO boot config fails")
set(NFC_ENABLE_STARTUP_DIAGNOSTICS 1 CACHE STRING "Run startup diagnostics (probe/status/selftest)")
set(NFC_ENABLE_BOOT_RW_TIMING_TEST 1 CACHE STRING "Run one-shot request-slot write/read/restore timing test at boot")
set(NFC_AUTO_POWER_OFF_MS 10000 CACHE STRING "Release power latch this many ms after boot/command (set 0 to disable)")
set(NFC_LED_SLOW_PERIOD_MS 800 CACHE STRING "Blink period for slow LED mode")
set(NFC_LED_FAST_PERIOD_MS 150 CACHE STRING "Blink period for fast LED mode")
set(NFC_TUNE_ADC_INPUT 1 CACHE STRING "ADC input index for tuning firmware (0..3)")
set(NFC_TUNE_SAMPLE_COUNT 256 CACHE STRING "Number of ADC samples per report in tuning firmware")
set(NFC_TUNE_REPORT_INTERVAL_MS 100 CACHE STRING "Report interval in ms for tuning firmware")
set(NFC_TUNE_SAMPLE_DELAY_US 20 CACHE STRING "Delay in microseconds between ADC samples in tuning firmware")
set(NFC_TUNE_EMA_ALPHA 0.50 CACHE STRING "EMA alpha for ADC smoothing in tuning firmware")
set(NFC_TUNE_BASELINE_SAMPLES 4000 CACHE STRING "ADC samples used for startup baseline capture in tuning firmware")
set(NFC_TUNE_FIELD_ON_MV 9 CACHE STRING "V_EH delta threshold (mV) to declare RF ON in tuning firmware")
set(NFC_TUNE_FIELD_OFF_MV 4 CACHE STRING "V_EH delta threshold (mV) to declare RF OFF in tuning firmware")
set(NFC_TUNE_DEBOUNCE_COUNT 1 CACHE STRING "Consecutive reports needed to confirm RF state changes in tuning firmware")
set(NFC_TUNE_BASELINE_TRACK_ALPHA 0.01 CACHE STRING "Baseline tracking alpha while RF is OFF in tuning firmware")
set(NFC_TUNE_FREEZE_BASELINE 1 CACHE STRING "Freeze baseline after startup capture (1=yes, 0=track while RF is OFF)")
set(NFC_TUNE_GRAPH_WIDTH 72 CACHE STRING "Graph width in samples for tuning terminal plot")
set(NFC_TUNE_GRAPH_HEIGHT 20 CACHE STRING "Graph height in rows for tuning terminal plot")
set(NFC_TUNE_GRAPH_MAX_MV 3000 CACHE STRING "Graph vertical scale max in mV for tuning terminal plot")
set(NFC_TUNE_BASELINE_SETTLE_MS 400 CACHE STRING "Settling delay before startup baseline capture in tuning firmware")
set(NFC_TUNE_EH_ENFORCE_RETRIES 8 CACHE STRING "Retry steps to enforce EH_EN=1 at boot/runtime")
set(NFC_TUNE_EH_ENFORCE_RETRY_MS 20 CACHE STRING "Delay (ms) between EH enforce retry steps")
set(NFC_TUNE_EH_RECHECK_MS 1000 CACHE STRING "Runtime interval (ms) to retry EH enable if EH_EN dropped")

set(NFC_COMMON_DEFS
  NFC_I2C_BAUDRATE_HZ=${NFC_I2C_BAUDRATE_HZ}
  NFC_I2C_SDA_PIN=${NFC_I2C_SDA_PIN}
  NFC_I2C_SCL_PIN=${NFC_I2C_SCL_PIN}
  NFC_STATUS_LED_PIN=${NFC_STATUS_LED_PIN}
  NFC_POWER_LED_PIN=${NFC_POWER_LED_PIN}
  NFC_POWER_HOLD_PIN=${NFC_POWER_HOLD_PIN}
  NFC_ST25_VCC_EN_PIN=${NFC_ST25_VCC_EN_PIN}
  NFC_ST25_POWER_ON_DELAY_MS=${NFC_ST25_POWER_ON_DELAY_MS}
  NFC_I2C_OP_TIMEOUT_US=${NFC_I2C_OP_TIMEOUT_US}
  NFC_ENABLE_WAKE_GPO_CONFIG=${NFC_ENABLE_WAKE_GPO_CONFIG}
  NFC_WAKE_GPO_SELFTEST_STRICT=${NFC_WAKE_GPO_SELFTEST_STRICT}
  NFC_ENABLE_STARTUP_DIAGNOSTICS=${NFC_ENABLE_STARTUP_DIAGNOSTICS}
  NFC_ENABLE_BOOT_RW_TIMING_TEST=${NFC_ENABLE_BOOT_RW_TIMING_TEST}
  NFC_AUTO_POWER_OFF_MS=${NFC_AUTO_POWER_OFF_MS}
  NFC_LED_SLOW_PERIOD_MS=${NFC_LED_SLOW_PERIOD_MS}
  NFC_LED_FAST_PERIOD_MS=${NFC_LED_FAST_PERIOD_MS}
)

target_compile_definitions(nfc_harness PRIVATE ${NFC_COMMON_DEFS})
target_compile_definitions(nfc_tune PRIVATE
  ${NFC_COMMON_DEFS}
  NFC_TUNE_ADC_INPUT=${NFC_TUNE_ADC_INPUT}
  NFC_TUNE_SAMPLE_COUNT=${NFC_TUNE_SAMPLE_COUNT}
  NFC_TUNE_REPORT_INTERVAL_MS=${NFC_TUNE_REPORT_INTERVAL_MS}
  NFC_TUNE_SAMPLE_DELAY_US=${NFC_TUNE_SAMPLE_DELAY_US}
  NFC_TUNE_EMA_ALPHA=${NFC_TUNE_EMA_ALPHA}
  NFC_TUNE_BASELINE_SAMPLES=${NFC_TUNE_BASELINE_SAMPLES}
  NFC_TUNE_FIELD_ON_MV=${NFC_TUNE_FIELD_ON_MV}
  NFC_TUNE_FIELD_OFF_MV=${NFC_TUNE_FIELD_OFF_MV}
  NFC_TUNE_DEBOUNCE_COUNT=${NFC_TUNE_DEBOUNCE_COUNT}
  NFC_TUNE_BASELINE_TRACK_ALPHA=${NFC_TUNE_BASELINE_TRACK_ALPHA}
  NFC_TUNE_FREEZE_BASELINE=${NFC_TUNE_FREEZE_BASELINE}
  NFC_TUNE_GRAPH_WIDTH=${NFC_TUNE_GRAPH_WIDTH}
  NFC_TUNE_GRAPH_HEIGHT=${NFC_TUNE_GRAPH_HEIGHT}
  NFC_TUNE_GRAPH_MAX_MV=${NFC_TUNE_GRAPH_MAX_MV}
  NFC_TUNE_BASELINE_SETTLE_MS=${NFC_TUNE_BASELINE_SETTLE_MS}
  NFC_TUNE_EH_ENFORCE_RETRIES=${NFC_TUNE_EH_ENFORCE_RETRIES}
  NFC_TUNE_EH_ENFORCE_RETRY_MS=${NFC_TUNE_EH_ENFORCE_RETRY_MS}
  NFC_TUNE_EH_RECHECK_MS=${NFC_TUNE_EH_RECHECK_MS}
)

target_link_libraries(nfc_harness
  pico_stdlib
  hardware_i2c
)
target_link_libraries(nfc_tune
  pico_stdlib
  hardware_i2c
  hardware_adc
)

if(PICO_CYW43_SUPPORTED)
  target_link_libraries(nfc_harness pico_cyw43_arch_none)
  target_link_libraries(nfc_tune pico_cyw43_arch_none)
  target_compile_definitions(nfc_harness PRIVATE NFC_HAS_CYW43=1)
  target_compile_definitions(nfc_tune PRIVATE NFC_HAS_CYW43=1)
else()
  target_compile_definitions(nfc_harness PRIVATE NFC_HAS_CYW43=0)
  target_compile_definitions(nfc_tune PRIVATE NFC_HAS_CYW43=0)
endif()

target_include_directories(nfc_harness PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/third_party/st25dv
)
target_include_directories(nfc_tune PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/third_party/st25dv
)

pico_enable_stdio_usb(nfc_harness 1)
pico_enable_stdio_uart(nfc_harness 0)
pico_enable_stdio_usb(nfc_tune 1)
pico_enable_stdio_uart(nfc_tune 0)

pico_add_extra_outputs(nfc_harness)
pico_add_extra_outputs(nfc_tune)
