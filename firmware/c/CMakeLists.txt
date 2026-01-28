cmake_minimum_required(VERSION 3.13)

# Default to a plain Pico; override via `-DPICO_BOARD=...` or `PICO_BOARD=...`.
if(NOT DEFINED PICO_BOARD)
  set(PICO_BOARD pico CACHE STRING "Pico board type (e.g. pico, pico_w)")
endif()

include(pico_sdk_import.cmake)

project(nfc_exploration C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(nfc_harness
  main.c
  third_party/st25dv.c
  third_party/st25dv_reg.c
)

set(NFC_I2C_BAUDRATE_HZ 100000 CACHE STRING "I2C baudrate in Hz")
set(NFC_I2C_SDA_PIN 4 CACHE STRING "I2C SDA GPIO (RP2040 pin number)")
set(NFC_I2C_SCL_PIN 5 CACHE STRING "I2C SCL GPIO (RP2040 pin number)")
set(NFC_STATUS_LED_PIN 15 CACHE STRING "Status LED GPIO (RP2040 pin number)")

target_compile_definitions(nfc_harness PRIVATE
  NFC_I2C_BAUDRATE_HZ=${NFC_I2C_BAUDRATE_HZ}
  NFC_I2C_SDA_PIN=${NFC_I2C_SDA_PIN}
  NFC_I2C_SCL_PIN=${NFC_I2C_SCL_PIN}
  NFC_STATUS_LED_PIN=${NFC_STATUS_LED_PIN}
)

target_link_libraries(nfc_harness
  pico_stdlib
  hardware_i2c
)

target_include_directories(nfc_harness PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/third_party
)

pico_enable_stdio_usb(nfc_harness 1)
pico_enable_stdio_uart(nfc_harness 0)

pico_add_extra_outputs(nfc_harness)
