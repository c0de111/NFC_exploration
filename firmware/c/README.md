# Firmware (Pico / RP2040)

This folder is a minimal **Pico SDK** scaffold for the NFC harness.

It uses:
- **Pico SDK** (external dependency): provides the toolchain integration + RP2040 hardware libraries (`hardware_i2c`, `pico_stdlib`, UF2 generation, …).
- **ST25DV driver** (vendored code): ST’s `stm32-st25dv` driver sources are included in this repo under `firmware/c/third_party/` via git subtree.

The ST driver is **not** part of the Pico SDK. It’s just compiled into the same firmware binary, and it talks to the Pico SDK I²C API through a small adapter layer (`ST25DV_IO_t`) implemented in `main.c`.

## Dependencies (host)
- CMake + Make/Ninja
- `arm-none-eabi-gcc` toolchain
- `pico-sdk` checkout on disk (default path: `$HOME/pico/pico-sdk`)

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

## How the Pico SDK + ST driver fit together
- **Build system**: `CMakeLists.txt` builds one target `nfc_harness` and links `pico_stdlib` + `hardware_i2c`. It also compiles the ST driver sources from `third_party/`.
- **Pico SDK include**: `pico_sdk_import.cmake` finds your local `pico-sdk` (via `PICO_SDK_PATH`) and pulls in its CMake helpers.
- **ST driver include**: `third_party/st25dv.c` and `third_party/st25dv_reg.c` are compiled into the firmware and their headers are added to the include path.
- **Bus adapter**: `main.c` provides `ST25DV_IO_t` callbacks (`Init/Read/Write/IsReady/GetTick`) implemented using Pico SDK I²C calls. The ST driver stays “platform-agnostic” and calls into these callbacks.
- **Driver entrypoints**: `main.c` uses `St25Dv_Drv.*` (the driver’s exported API table). Some internal `ST25DV_*` functions exist in the sources but are not declared in the public header.

### Addressing note (7-bit vs 8-bit)
ST’s driver uses “8-bit I²C addresses” like `0xA6/0xAE` (includes the R/W bit in the LSB).
Pico SDK expects a **7-bit** address. The adapter shifts by 1 (`addr7 = addr8 >> 1`).

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
