# Firmware (Pico / RP2040)

This folder is a minimal **Pico SDK** scaffold for the NFC harness.

## Build
```bash
cd firmware/c
./build.sh
```

You can override common settings via CMake cache variables:
```bash
cd firmware/c
./build.sh --clean
cmake -S . -B build -DPICO_BOARD=pico -DNFC_I2C_SDA_PIN=4 -DNFC_I2C_SCL_PIN=5 -DNFC_STATUS_LED_PIN=15
cmake --build build -j
```

## Flash
- **USB drag‑and‑drop**: copy `build/nfc_harness.uf2` onto the Pico in BOOTSEL mode.
- **SWD (OpenOCD + CMSIS‑DAP probe)**:
  ```bash
  cd firmware/c
  ./flash.sh
  ```

## What it does
`main.c` initializes I²C and repeatedly scans for devices. For ST25DV, you typically expect:
- `0x53` (user memory + dynamic regs)
- `0x57` (system memory)

