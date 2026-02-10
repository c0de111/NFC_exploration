# Firmware (Pico / RP2040)

Pico-only firmware scaffold for the NFC harness.

It uses:
- **Pico SDK** (external dependency): toolchain integration + RP2040 libs (`hardware_i2c`, `pico_stdlib`, UF2 generation, …).
- **ST25DV driver** (vendored code): ST’s `stm32-st25dv` sources live in `third_party/st25dv/` (git subtree).

The ST driver is **not** part of the Pico SDK. It’s compiled into the firmware and talks to the Pico SDK I²C API through a small adapter layer (`ST25DV_IO_t`) implemented in `src/main.c`.

## Layout
```
firmware/
  src/               # app entrypoint + bus adapter
  include/           # project headers (empty placeholder for now)
  third_party/st25dv # vendored ST25DV driver (subtree)
  cmake/             # pico_sdk_import.cmake
  scripts/           # build/flash helpers
  build/             # generated (gitignored)
```

## Dependencies (host)
- CMake + Make/Ninja
- `arm-none-eabi-gcc` toolchain
- `pico-sdk` checkout on disk (default path: `$HOME/pico/pico-sdk`)

## Build
```bash
# from repo root
make firmware

# or inside firmware/
cd firmware
./scripts/build.sh
```

Picotool is disabled by default to avoid fetching/building it. On first run the build script downloads `uf2conv.py` and `uf2families.json` (Microsoft UF2 tool) into `firmware/scripts/` and converts `nfc_harness.bin` → `nfc_harness.uf2` without picotool. To re-enable picotool-based UF2/signing instead, configure CMake with `-DPICO_NO_PICOTOOL=OFF -DPICO_NO_UF2=OFF`.

Clean build directory (no rebuild):
```bash
make clean
# or
cd firmware && ./scripts/build.sh --clean
```

You can override common settings via CMake cache variables:
```bash
cd firmware
cmake -S . -B build -DPICO_BOARD=pico -DNFC_I2C_SDA_PIN=4 -DNFC_I2C_SCL_PIN=5 -DNFC_STATUS_LED_PIN=15 -DNFC_ST25_VCC_EN_PIN=18 -DNFC_ST25_POWER_ON_DELAY_MS=10 -DNFC_ENABLE_STARTUP_DIAGNOSTICS=1 -DNFC_ENABLE_BOOT_RW_TIMING_TEST=1
cmake --build build -j
```

By default the firmware drives `NFC_ST25_VCC_EN_PIN` high (`GP18`) and waits `NFC_ST25_POWER_ON_DELAY_MS` before ST25 I2C accesses. Set `-DNFC_ST25_VCC_EN_PIN=-1` if ST25 VCC is always powered externally.
At boot it also runs diagnostics (I2C address probe, dynamic status registers, self-test summary). Set `-DNFC_ENABLE_STARTUP_DIAGNOSTICS=0` for production-style minimal boot logs. `NFC_ENABLE_BOOT_RW_TIMING_TEST=1` enables a one-shot request-slot write/read/restore timing test (auto-skipped if a live `INKI` request is present).

## Flash
- **USB drag‑and‑drop**: copy `build/nfc_harness.uf2` onto the Pico in BOOTSEL mode.
- **SWD (OpenOCD + CMSIS‑DAP probe)**:
  ```bash
  cd firmware
  ./scripts/flash.sh
  ```

## What it does
`main.c` initializes I²C, brings up the ST25DV driver, reads the last 16 bytes of user memory (where the Android app writes the `INKI` request), prints it, and clears it when present.

For ST25DV, you typically expect these 7‑bit I²C addresses:
- `0x53` (user memory + dynamic regs)
- `0x57` (system memory)

## How the Pico SDK + ST driver fit together
- **Build system**: `CMakeLists.txt` builds one target `nfc_harness` and links `pico_stdlib` + `hardware_i2c`. It also compiles the ST driver sources from `third_party/st25dv/`.
- **Pico SDK include**: `cmake/pico_sdk_import.cmake` finds your local `pico-sdk` (via `PICO_SDK_PATH`) and pulls in its CMake helpers.
- **ST driver include**: `third_party/st25dv/st25dv.c` and `third_party/st25dv/st25dv_reg.c` are compiled into the firmware and their headers are added to the include path.
- **Bus adapter**: `src/main.c` provides `ST25DV_IO_t` callbacks (`Init/Read/Write/IsReady/GetTick`) implemented using Pico SDK I²C calls. The ST driver stays “platform-agnostic” and calls into these callbacks.
- **Driver entrypoints**: `src/main.c` uses `St25Dv_Drv.*` (the driver’s exported API table). Some internal `ST25DV_*` functions exist in the sources but are not declared in the public header.

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
git subtree pull --prefix=firmware/third_party/st25dv /home/nicolas/github/stm32-st25dv v2.0.2 --squash
```
