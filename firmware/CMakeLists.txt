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

set(NFC_I2C_BAUDRATE_HZ 100000 CACHE STRING "I2C baudrate in Hz")
set(NFC_I2C_SDA_PIN 20 CACHE STRING "I2C SDA GPIO (RP2040 pin number)")
set(NFC_I2C_SCL_PIN 21 CACHE STRING "I2C SCL GPIO (RP2040 pin number)")
set(NFC_STATUS_LED_PIN 15 CACHE STRING "Status LED GPIO (RP2040 pin number)")
set(NFC_POWER_LED_PIN 25 CACHE STRING "Power LED GPIO (set -1 to disable)")
set(NFC_ST25_VCC_EN_PIN 18 CACHE STRING "GPIO used to enable ST25 VCC (set -1 to disable)")
set(NFC_ST25_POWER_ON_DELAY_MS 10 CACHE STRING "Delay after enabling ST25 VCC before I2C access")
set(NFC_I2C_OP_TIMEOUT_US 20000 CACHE STRING "I2C op timeout in microseconds for read/write/probe transactions")
set(NFC_ENABLE_STARTUP_DIAGNOSTICS 1 CACHE STRING "Run startup diagnostics (probe/status/selftest)")
set(NFC_ENABLE_BOOT_RW_TIMING_TEST 1 CACHE STRING "Run one-shot request-slot write/read/restore timing test at boot")

target_compile_definitions(nfc_harness PRIVATE
  NFC_I2C_BAUDRATE_HZ=${NFC_I2C_BAUDRATE_HZ}
  NFC_I2C_SDA_PIN=${NFC_I2C_SDA_PIN}
  NFC_I2C_SCL_PIN=${NFC_I2C_SCL_PIN}
  NFC_STATUS_LED_PIN=${NFC_STATUS_LED_PIN}
  NFC_POWER_LED_PIN=${NFC_POWER_LED_PIN}
  NFC_ST25_VCC_EN_PIN=${NFC_ST25_VCC_EN_PIN}
  NFC_ST25_POWER_ON_DELAY_MS=${NFC_ST25_POWER_ON_DELAY_MS}
  NFC_I2C_OP_TIMEOUT_US=${NFC_I2C_OP_TIMEOUT_US}
  NFC_ENABLE_STARTUP_DIAGNOSTICS=${NFC_ENABLE_STARTUP_DIAGNOSTICS}
  NFC_ENABLE_BOOT_RW_TIMING_TEST=${NFC_ENABLE_BOOT_RW_TIMING_TEST}
)

target_link_libraries(nfc_harness
  pico_stdlib
  hardware_i2c
)

if(PICO_CYW43_SUPPORTED)
  target_link_libraries(nfc_harness pico_cyw43_arch_none)
  target_compile_definitions(nfc_harness PRIVATE NFC_HAS_CYW43=1)
else()
  target_compile_definitions(nfc_harness PRIVATE NFC_HAS_CYW43=0)
endif()

target_include_directories(nfc_harness PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/third_party/st25dv
)

pico_enable_stdio_usb(nfc_harness 1)
pico_enable_stdio_uart(nfc_harness 0)

pico_add_extra_outputs(nfc_harness)
