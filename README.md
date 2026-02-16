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
- Firmware command opcode handling is active:
  - `0x11` -> `LED1` slow blink
  - `0x12` -> `LED1` fast blink (same onboard LED)
  - `0x01` -> legacy alias mapped to `LED1` slow blink
- Incomplete/invalid `INKI` frames are ignored (to avoid partial-write races).
- Valid requests are cleared only after `RF field` goes `OFF` (reduces interference with phone-side readback).
- Power latch is asserted early by Pico on boot (`GP28`) and auto-released after `NFC_AUTO_POWER_OFF_MS` (default `10000` ms).
- Runtime logging is event-driven: startup diagnostics once, then RF field edges and request actions only.

## Current Wake/Latch Circuitry (PCB: `NFC_harness_V0`)
- `Q1` (`TSM260P02CX`, P-MOS) is the high-side power switch:
  - source -> `Vbatt`
  - drain -> Pico `VSYS`
- ST25 `GPO_(OPEN_DRAIN)` and the `Q1` gate share the same control net (wake/power-on net).
- Gate network shaping:
  - `R4=1M` pull-up to `Vbatt`
  - `C5=22n` to GND (pulse shaping / hold)
- Pico latch-hold path:
  - Pico `GPIO28` (`NFC_POWER_HOLD_PIN`) -> `R5=100k` -> `Q2` base
  - `R9=50k` pulls base to GND
  - `Q2` collector also sinks the same gate-control net
- On this PCB revision, Pico `GPIO19` is unconnected.
- LED wiring status on this PCB revision:
  - Both blink commands (`0x11` slow, `0x12` fast) use the firmware power-LED output (onboard Pico/Pico W path).
  - `NFC_STATUS_LED_PIN` (default `GP15`) is currently not routed to a dedicated onboard LED footprint here (external LED/wire may be needed).

## Command Flow (Phone -> ST25 -> Pico)
1. Board is fully off (`Vbatt` connected, no USB, Pico unpowered).
2. Phone tap powers ST25 RF side and writes the 16-byte `INKI` payload (including opcode byte) over NFC-V into ST25 EEPROM, while Pico can still be fully off.
3. ST25 asserts `GPO` low on configured wake event (`RF_WRITE`), pulling `Q1` gate low and powering Pico.
4. Pico boots and immediately asserts `GPIO28` high to latch power through `Q2`.
5. Data transfer from ST25 to Pico happens after boot over I2C (not on GPO):
   - Pico powers ST25 VCC via `GP18`
   - firmware reads request bytes from the last ST25 user-memory slot
6. Firmware validates the payload, applies opcode behavior, queues clear for RF-OFF, and later clears the slot.
7. Firmware releases the latch after timeout (`NFC_AUTO_POWER_OFF_MS`, default `10000` ms; re-armed on accepted commands).

Note:
- Runtime-loop request processing still runs while RF field is ON, with clear deferred to RF field OFF.
- Additionally, at boot firmware now checks for a stored valid request. If RF field is already OFF, it applies the command immediately and clears it at boot.
- If no valid `INKI` request is found after wake and RF is already OFF, firmware releases the power latch immediately.

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
Recommended Android workflow on this repo is CLI build plus sideload copy: run `./android/scripts/build_and_sync_apk.sh --app tap` from repo root (optionally add `--sdk-dir ~/Android/Sdk`). The script builds the debug APK from `android/InkiNfcTapToBook_androidstudio/`, auto-detects Android Studio JBR/SDK when available, and copies the APK to `~/Sync` by default so it can be installed on the phone without USB debugging. Full details and options are in `android/README.md`.

## Quick RF test (phone + Pico)
1. Connect a loop antenna to `J3` (`AC0`/`AC1`) on the PCB.
2. Build and flash firmware:
   - `make firmware`
   - copy `firmware/build/nfc_harness.uf2` to Pico in BOOTSEL mode (or use SWD).
3. Open serial logs:
   - `sudo tio /dev/ttyACM0`
4. On phone, open the NFC-V app (`InkiNfcTapToBook`):
   - pick command button: `LED1 Slow` or `LED2 Fast`
   - enable `Write on tap`
5. Tap phone to antenna and hold briefly.

Expected runtime log pattern:
- `RF field: ON`
- `Request@0x01F0: ...`
- `INKI request: version=... opcode=... duration_min=...`
- `Command applied: opcode=... -> ...`
- `Queued clear after RF field OFF`
- `RF field: OFF`
- `Cleared request after RF OFF (...)`
- `Auto power-off: GP28 -> LOW`

Sample serial log (successful run):
```text
[INFO] RF field: OFF
[INFO] RF field: ON
[INFO] Request@0x01F0: 49 4E 4B 49 01 11 3C 00 ...
[OK] INKI request: version=1 opcode=0x11 duration_min=60 unix=... nonce=...
[OK] Command applied: opcode=0x11 -> LED1 slow blink
[INFO] Queued clear after RF field OFF
[INFO] RF field: OFF
[OK] Cleared request after RF OFF (16 bytes @ 0x01F0, attempts=1, 8ms)
[WARN] Auto power-off: GP28 -> LOW
```
