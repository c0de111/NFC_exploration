# Firmware (Pico / RP2040)

This folder is a minimal **Pico SDK** scaffold for the NFC harness.

It vendors ST’s ST25DV I²C driver (from `STMicroelectronics/stm32-st25dv`) as a git subtree in `firmware/c/third_party/`.

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
`main.c` initializes I²C, brings up the ST25DV driver, reads the last 16 bytes of user memory (where the Android app writes the `INKI` request), prints it, and clears it when present.

For ST25DV, you typically expect these 7‑bit I²C addresses:
- `0x53` (user memory + dynamic regs)
- `0x57` (system memory)

## Updating the ST driver subtree
This repo vendors the driver as a **squashed git subtree** (not a submodule).

Upstream:
- Local clone: `/home/nicolas/github/stm32-st25dv`
- Upstream URL: `https://github.com/STMicroelectronics/stm32-st25dv`
- Currently imported tag: `v2.0.2`

Update command:
```bash
git subtree pull --prefix=firmware/c/third_party /home/nicolas/github/stm32-st25dv v2.0.2 --squash
```
