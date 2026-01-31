# NFC exploration

Internal sandbox repo to explore **ST25DV04KC (ISO15693 / NFC-V)** tap-to-trigger flows for inki.

## Goal
Build a small, reproducible test platform (ST25DV04KC + **Raspberry Pi Pico/RP2040** + antenna + simple enclosure) to validate:
- RF-field / `V_EH` wake behavior in the real case
- Phone → tag writes (EEPROM / Type 5)
- MCU reads request over I²C and clears/acks
- End-to-end timing assumptions for a future “tap-to-book” workflow

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
