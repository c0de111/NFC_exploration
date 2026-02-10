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
- 2026-02-10: Hardened boot request-slot R/W timing test against transient EEPROM busy/NACK windows: added bounded retry helpers for ST25 data read/write around verify/restore phases, guaranteed best-effort backup restore on error paths, and added explicit warning when boot snapshot equals the known self-test pattern (`A5 98 DF 12 51 94 CB 0E 4D 80 C7 3A 79 BC F3 36`) left by earlier failed boots.
- 2026-02-10: Fixed false-positive ST25 "ready" probing on RP2040: `i2c_address_acks()` previously used a zero-length `i2c_write_blocking()` call, which can return success without a real bus transaction in this SDK path. Updated probe/readiness checks to send a real 2-byte address pointer (`0x0000`) so ACK/NACK reflects actual device availability (important for EEPROM write-cycle busy handling and boot R/W timing test correctness).
- 2026-02-10: Refactored `firmware/src/main.c` startup flow for readability and production gating: split boot path into focused subroutines (`boot_log_banner`, power/LED init, bus registration, identity/memory bring-up, optional diagnostics, steady-state poll loop), introduced `HarnessState`/`StartupDiag` structs, and added CMake-exposed `NFC_ENABLE_STARTUP_DIAGNOSTICS` (default `1`) so startup diagnostics can be disabled cleanly for production builds.
- 2026-02-10: Added boot-time ST25 diagnostics in firmware output with ANSI colors (inki-style): explicit I2C ACK probe (`0x53`, `0x57`), dynamic status register dump (I2C session, EH/RF/VCC, IT status, GPO, mailbox), `SELFTEST: PASS/FAIL` summary with reason codes, and a guarded one-shot request-slot write/read/restore timing test (`NFC_ENABLE_BOOT_RW_TIMING_TEST`, default `1`, auto-skip when a live `INKI` request is present).
- 2026-02-10: ST25 I2C communication confirmed end-to-end after GP18 power enable: `ICREF=0x50`, `Memory=128 blocks * 4 bytes = 512 bytes` (ST25DV04KC capacity), and stable reads from request slot `0x01F0`. The previous `St25Dv_Drv.Init failed: -1` was traced to an outdated ICREF whitelist in the vendored ST driver; updated to accept KC IDs (`0x50/0x51`) and check `ReadID()` return status.
- 2026-02-10: USB serial bring-up confirmed on `/dev/ttyACM0` (boot banner + periodic logs). Observed `St25Dv_Drv.Init/ReadID/ReadMemSize failed: -1` with `Memory too small for request check` while ST25 VCC switch pin (GP18) was low. Firmware updated to drive ST25 VCC enable pin high before I2C init (new CMake options: `NFC_ST25_VCC_EN_PIN`, default `18`, and `NFC_ST25_POWER_ON_DELAY_MS`, default `10`).
- 2026-02-08: Protocol for this work: PCB will be milled in‑house, keep Pico W SMD module footprint, and keep external antenna (no PCB antenna). These defaults should be treated as the standing plan for future steps.
- 2026-02-08: Decision for first milling pass: ignore the DRC `items_not_allowed` errors from the Pico W RF keepout (keepout is inside the footprint); proceed while leaving this DRC warning.
- 2026-02-08: Decision for first milling pass: ignore remaining DRC `unconnected_items` between GND zones (board judged OK by inspection); proceed while leaving this DRC error.
- 2026-02-08: Decision for first milling pass: accept IC1/C3 courtyard overlap as‑is (no reposition yet); proceed while leaving this DRC error.
- 2026-02-08: Quick PCB readiness check with `kicad-cli` on `pcb/NFC_harness_V0/` found DRC issues that block fab: board outline not closed + self‑intersections, edge clearance violations around the Pico W footprint, keepout violations in the Pico W RF keepout, and 3 unconnected GND items (likely due to outline/zone fill). Courtyard overlap between IC1 and C3 also flagged. Schematic netlist shows Pico W module (A1) with ST25DV on GP20/GP21 (I²C), GP18 as ST25 VCC switch, GP19 as GPO, antenna on J3 with C2+C3 tuning, and V_EH on J1. Firmware defaults to GP4/GP5 and does not drive GP18, so pins/power control must be overridden or the PCB adjusted.
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
