# context – NFC_exploration

Sandbox repo to explore **ST25DV04KC (ISO15693 / NFC‑V)** “tap‑to‑trigger / tap‑to‑book” flows for inki. This file is the single source for detailed notes, history, and solved problems.

## Goal
Validate an end‑to‑end flow where:
- a phone writes a small “request” into ST25DV EEPROM over RF (tag VCC can be off)
- the device wakes (via RF field / EH / GPO strategy)
- MCU reads the request over I²C, acts on it, then clears/acks and sleeps again

## Key artifacts
- ST docs:
  - `datasheet/st25dv04kc.pdf`
  - `datasheet/an5733-using-st25dvi2c-series-for-ultralow-power-applications-stmicroelectronics.pdf`
- Android:
  - Drop‑in source: `android/inki_nfc_tap_to_book/`
  - Android Studio project: `android/InkiNfcTapToBook_androidstudio/`
  - Request format: `android/inki_nfc_tap_to_book/REQUEST_FORMAT.md`
  - “Hello” sanity app: `android/InkiHello_androidstudio/`
- Firmware:
  - `firmware/src/main.c` (Pico/RP2040 I²C harness)
- KiCad harness starter:
  - `pcb/NFC_harness_V0/`
- ST25DV I²C driver (vendored):
  - `firmware/third_party/st25dv/` (git subtree of `STMicroelectronics/stm32-st25dv`)

## Firmware status (Pico/RP2040)
- `firmware/src/main.c` reads the last 16 bytes of ST25DV user memory (Android “INKI” request payload), prints it, and clears it when present.
- Build (Pico SDK):
  ```bash
  cd firmware
  ./scripts/build.sh
  ```

## ST25DV04KC: ultra‑low power notes (AN5733 + datasheet)
### Power strategy in one sentence
Power ST25DV VCC only when the MCU needs I²C; keep RF functionality when VCC is off; use GPO/field events to wake MCU if desired.

### Key points pulled from AN5733
- VCC can be powered directly from an MCU GPIO (<200 µA @ 1.8 V).
- Best-case uses **LPD** (10-ball/12-pin packages) before removing VCC to avoid reboot during RF EEPROM programming.
- If using **8-pin** package (no LPD): slow VCC edges (≥10 µF on VCC) and keep **GPO pulled up permanently** (open-drain) so RF interrupts can be signaled.

### I²C addressing (datasheet, default)
- Device selects: `A6h/A7h` (user+dynamic), `AEh/AFh` (system)
- 7-bit addresses typically: `0x53` (user/dynamic), `0x57` (system)

## Android app: `InkiNfcTapToBook`
- Uses Android reader mode + `NfcV` for ISO15693 tags.
- UI: log view, `Write on tap` toggle, `Clear log`.
- ISO15693 commands: Get System Info `0x2B`, Read Single Block `0x20`, Write Single Block `0x21`.
- Main activity: `android/InkiNfcTapToBook_androidstudio/app/src/main/java/.../MainActivity.kt`
- Drop‑in sources: `android/inki_nfc_tap_to_book/` (same logic, easier to copy).
- APK outputs: `android/InkiNfcTapToBook_androidstudio/app/build/outputs/apk/debug/app-debug.apk`, `android/InkiHello_androidstudio/app/build/outputs/apk/debug/app-debug.apk`.

## What to do next
- Define harness pinout (I²C pins, optional status LED) and implement minimal ST25DV read/write helpers for request blocks.
- Add first “ST25 explore” schematic to `pcb/NFC_harness_V0/` based on AN5733 + datasheet.
- Validate on real tag: RF writes, wake behavior, timing, robustness.

## Log (chronological)
- 2026-01-25: Repo scaffolded (AVR build/flash scripts + KiCad/CAM template copied and renamed to `NFC_harness_V0`).
- 2026-01-25: Added Android NFC‑V writer app sources under `android/inki_nfc_tap_to_book/` and created Android Studio projects for sideload tests.
- 2026-01-25: Travel notes: no ISO15693 tag available. Hello APK installs cleanly; NFC APK can trigger Play Protect scan but installs after scan.
- 2026-01-27: Added notes from AN5733 + ST25DV04KC datasheet; reviewed ST’s `stm32-st25dv` driver (good for Pico, too big for ATtiny202 as‑is).
- 2026-01-28: Switched harness firmware to Pico/RP2040 (Pico SDK workflow) and removed AVR/ATtiny scaffolding. Vendored `stm32-st25dv` as git subtree and wired into Pico harness firmware.

## Historical notes (from former `codex.md`)
- Plan: bring up ST25DV04KC + Pico/RP2040 harness (power, I²C, antenna); validate phone → EEPROM writes (NfcV); implement MCU-side request parsing + clearing; measure timing/range with enclosure/ferrite.
- Chat log snippets:
  - 2026-01-25: Repo scaffolded from PlaygroundSentinel workflow (AVR scripts + KiCad template renamed to NFC_harness_V0).
  - 2026-01-25: Added minimal Android NFC-V writer drop-in (`android/inki_nfc_tap_to_book/`), built sideload APKs (hello + NFC app).
  - 2026-01-25: Travel notes: no ISO15693 tag available; Play Protect scan may appear but install succeeds; app logs only on-screen.
  - 2026-01-28: Switched harness firmware plan to Pico/RP2040; removed AVR/ATtiny scaffolding.
