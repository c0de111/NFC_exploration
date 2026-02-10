# NFC exploration

Internal sandbox repo to explore **ST25DV04KC (ISO15693 / NFC-V)** tap-to-trigger flows for inki.

## Goal
Build a small, reproducible test platform (ST25DV04KC + **Raspberry Pi Pico/RP2040** + antenna + simple enclosure) to validate:
- RF-field / `V_EH` wake behavior in the real case
- Phone → tag writes (EEPROM / Type 5)
- MCU reads request over I²C and clears/acks
- End-to-end timing assumptions for a future “tap-to-book” workflow

## Current validated behavior
- ST25DV04KC bring-up over I2C is stable on Pico (`GP20/GP21`), with ST25 power switched by `GP18`.
- End-to-end NFC-V write from phone to tag user memory is working.
- Firmware parses 16-byte `INKI` request payloads from the last user-memory slot (`0x01F0` on ST25DV04KC).
- Incomplete/invalid `INKI` frames are ignored (to avoid partial-write races).
- Valid requests are cleared only after `RF field` goes `OFF` (reduces interference with phone-side readback).
- Runtime logging is event-driven: startup diagnostics once, then RF field edges and request actions only.

## Repository layout
- `docs/context.md` – running notes, decisions, history, and solved problems
- `firmware/` – Pico SDK firmware scaffold (RP2040) for ST25 I²C exploration  
  - `src/` app entry (`main.c`) and bus adapter  
  - `third_party/st25dv/` vendored ST25DV driver (git subtree)  
  - `scripts/` build/flash/reset helpers  
  - `cmake/` Pico SDK import helper  
- `pcb/NFC_harness_V0/` – KiCad project (schematic, layout, CAM profile)
- `pcb/tools/run_pcb2gcode.sh` – shared pcb2gcode runner
- `pcb/components/` + `pcb/project-libraries/` – KiCad symbols/footprints (includes ST25DV package lib submodule)
- `datasheet/` – ST25DV04KC datasheet + app notes
- `android/` – NFC‑V test apps (writer + hello) *(not needed for Pico-only workflow)*

## Typical workflow (from repo root)
1) **Build firmware**: `make firmware` (uses `PICO_SDK_PATH`, default `$HOME/pico/pico-sdk`).
2) **Flash**:  
   - Drag‑and‑drop: copy `firmware/build/nfc_harness.uf2` to Pico in BOOTSEL mode, or  
   - SWD: `make firmware-flash` (OpenOCD + CMSIS-DAP).
3) **Iterate**: edit `firmware/src/main.c` (and future helpers), rebuild, and re‑flash.
4) **CAM / milling** (optional): `pcb/tools/run_pcb2gcode.sh -b NFC_harness_V0` to regenerate toolpaths.

## Build & flash (Pico / RP2040)

### Build
```bash
# from repo root (recommended)
make firmware

# or inside firmware/
cd firmware
./scripts/build.sh   # expects $PICO_SDK_PATH (defaults to $HOME/pico/pico-sdk)
```
More detail (pin overrides, UF2 path, picotool toggles): see `firmware/README.md`.

Notes:
- Picotool is disabled by default to avoid downloads. The build script auto-fetches `uf2conv.py` (Microsoft UF2 tool) on first run and converts `nfc_harness.bin` → `nfc_harness.uf2`.
- Set `-DPICO_NO_PICOTOOL=OFF` in `cmake` if you prefer the picotool UF2/signing path.

Clean build artifacts:
```bash
make clean   # removes firmware/build only
```

Flash options:
- **USB drag‑and‑drop**: copy `firmware/build/nfc_harness.uf2` to the Pico mass‑storage device (BOOTSEL).
- **SWD (OpenOCD + CMSIS‑DAP probe)**:
  ```bash
  cd firmware
  ./scripts/flash.sh
  ```

### Root make targets (quick reference)
- `make firmware` – configure & build firmware into `firmware/build/`
- `make firmware-flash` – flash over SWD (OpenOCD + CMSIS‑DAP)
- `make clean` – remove `firmware/build/` only

## CAM workflow (pcb2gcode)
From repo root:
```bash
pcb/tools/run_pcb2gcode.sh -b NFC_harness_V0
```
The profile lives at `pcb/NFC_harness_V0/cam/pcb2gcode/profiles/default.millprojects` and uses repo-relative paths.

## Android NFC‑V app
See `android/README.md`.

## Quick RF test (phone + Pico)
1. Connect a loop antenna to `J3` (`AC0`/`AC1`) on the PCB.
2. Build and flash firmware:
   - `make firmware`
   - copy `firmware/build/nfc_harness.uf2` to Pico in BOOTSEL mode (or use SWD).
3. Open serial logs:
   - `sudo tio /dev/ttyACM0`
4. On phone, open the NFC-V app (`InkiNfcTapToBook`) and enable `Write on tap`.
5. Tap phone to antenna and hold briefly.

Expected runtime log pattern:
- `RF field: ON`
- `Request@0x01F0: ...`
- `INKI request: version=... opcode=... duration_min=...`
- `Queued clear after RF field OFF`
- `RF field: OFF`
- `Cleared request after RF OFF (...)`

Sample serial log (successful run):
```text
[BOOT] NFC harness (RP2040/Pico) boot
[INFO] I2C: SDA=GP20 SCL=GP21 @ 100000 Hz
[INFO] ST25 power: GP18 -> HIGH (wait 10 ms)
[OK] St25Dv_Drv.Init OK
[INFO] ICREF: 0x50 (ST25DV04KC)
[INFO] Memory: blocks=128 bytes_per_block=4 total=512 bytes
[INFO] Detected part by capacity: ST25DV04KC
[INFO] ICREV: 0x13
[INFO] UID: E0025068D0905648
[INFO] UID product code: 0x50 (ST25DV04KC-IE)
[OK] Diag: I2C link OK (ID + memory map reads succeeded)
[INFO] Expected ST25DV addresses (7-bit): 0x53 (user/dynamic), 0x57 (system)
[INFO] I2C probe (7-bit): 0x53=ACK 0x57=ACK
[OK] I2C address probe passed
[INFO] DYN I2C session: CLOSED
[INFO] DYN EH: EH_EN=OFF EH_ON=OFF FIELD_ON=OFF VCC_ON=ON
[INFO] DYN RF field: OFF
[INFO] DYN VCC seen by ST25: ON
[INFO] DYN RF mngt: RF_DISABLE=OFF RF_SLEEP=OFF
[INFO] DYN IT status: 0x00
[INFO] DYN GPO: 0x01
[INFO] DYN mailbox: MBEN=0 HOSTPUT=0 RFPUT=0 HOSTMISS=0 RFMISS=0 CUR=NO_MSG
[INFO] DYN mailbox length: 0
[OK] Dynamic status diagnostics passed
[INFO] Request@0x01F0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[OK] Boot R/W timing passed: write=9ms verify=2ms restore=8ms (16 bytes @ 0x01F0)
[OK] SELFTEST: PASS
[INFO] RF field: OFF
[INFO] RF field: ON
[INFO] Request@0x01F0: 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
[INFO] RF request slot is empty
[INFO] Request@0x01F0: 49 4E 4B 49 01 01 3C 00 96 8D 8B 69 0D 35 0B 41
[OK] INKI request: version=1 opcode=0x01 duration_min=60 unix=1770753430 nonce=0x410B350D
[INFO] Queued clear after RF field OFF
[INFO] RF field: OFF
[OK] Cleared request after RF OFF (16 bytes @ 0x01F0, attempts=1, 8ms)
```
