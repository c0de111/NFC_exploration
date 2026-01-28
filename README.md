# NFC exploration

Internal sandbox repo to explore **ST25DV04KC (ISO15693 / NFC-V)** tap-to-trigger flows for inki.

## Goal
Build a small, reproducible test platform (ST25DV04KC + **Raspberry Pi Pico/RP2040** + antenna + simple enclosure) to validate:
- RF-field / `V_EH` wake behavior in the real case
- Phone → tag writes (EEPROM / Type 5)
- MCU reads request over I²C and clears/acks
- End-to-end timing assumptions for a future “tap-to-book” workflow

## Repository layout
- `context.md` – running notes, decisions, and current status
- `firmware/c/` – Pico SDK firmware scaffold (RP2040) for ST25 I²C exploration
  - `firmware/c/third_party/` – vendored ST25DV I²C driver (`stm32-st25dv`) via git subtree
- `pcb/NFC_harness_V0/` – KiCad starting point copied from the proven Sentinel V2 workflow (CAM + pcb2gcode profiles)
- `pcb/tools/run_pcb2gcode.sh` – shared pcb2gcode runner
- `pcb/components/` – shared KiCad libs referenced by the project
- `android/` – Android apps for NFC‑V/ISO15693 testing (writer app + sideload workflow)

## Build & flash (Pico / RP2040)

Build (Pico SDK):
```bash
cd firmware/c
./build.sh
```

Flash options:
- **USB drag‑and‑drop**: copy `firmware/c/build/nfc_harness.uf2` to the Pico mass‑storage device (BOOTSEL).
- **SWD (OpenOCD + CMSIS‑DAP probe)**:
  ```bash
  cd firmware/c
  ./flash.sh
  ```

## CAM workflow (pcb2gcode)
From repo root:
```bash
pcb/tools/run_pcb2gcode.sh -b NFC_harness_V0
```
The profile lives at `pcb/NFC_harness_V0/cam/pcb2gcode/profiles/default.millprojects` and uses repo-relative paths.

## Android NFC‑V app
See `android/README.md`.
