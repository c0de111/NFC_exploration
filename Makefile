MCU = attiny202
F_CPU = 20000000UL
CC = avr-gcc
OBJCOPY = avr-objcopy
SIZE = avr-size

APP ?= nfc_harness
SRC_DIR := firmware/$(APP)
BUILD_DIR := build/$(APP)
TARGET := $(APP)

CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -Wall
LDFLAGS = -mmcu=$(MCU)

ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex

all: $(HEX)

$(BUILD_DIR):
	@mkdir -p "$@"

$(ELF): $(SRC_DIR)/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $<

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

size: $(ELF)
	$(SIZE) -C --mcu=$(MCU) $<

clean:
	rm -rf build

nfc_harness:
	$(MAKE) APP=$@ all

.PHONY: all clean size nfc_harness
