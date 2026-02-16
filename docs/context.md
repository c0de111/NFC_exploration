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

## Android app: `InkiNfcTapToBook` (launcher label: `inki`)
- Uses Android reader mode + `NfcV` for ISO15693 tags.
- UI: command buttons, write status line, and log view (write is always on tag tap).
- ISO15693 commands: Get System Info `0x2B`, Read Single Block `0x20`, Write Single Block `0x21`.
- Main activity: `android/InkiNfcTapToBook_androidstudio/app/src/main/java/.../MainActivity.kt`
- Drop‑in sources: `android/inki_nfc_tap_to_book/` (same logic, easier to copy).
- APK outputs: `android/InkiNfcTapToBook_androidstudio/app/build/outputs/apk/debug/app-debug.apk`, `android/InkiHello_androidstudio/app/build/outputs/apk/debug/app-debug.apk`.

## What to do next
- Define harness pinout (I²C pins, optional status LED) and implement minimal ST25DV read/write helpers for request blocks.
- Add first “ST25 explore” schematic to `pcb/NFC_harness_V0/` based on AN5733 + datasheet.
- Validate on real tag: RF writes, wake behavior, timing, robustness.

## Log (chronological)
- 2026-02-16: Updated Android tap app UX defaults for production tap flow:
  - App label changed to `inki`.
  - Removed `Write on tap` toggle from UI/logic; app now always writes on tag tap.
  - Enlarged command buttons (`LED1 Slow`, `LED2 Fast`) for easier touch interaction.
  - Kept explicit state feedback (`Ready - Tap to write` -> `Writing...` -> `Done` / `No Success - Try again!`) and mirrored changes in both source copies.
- 2026-02-16: Updated root Android helper `build_android.sh` to enforce local workflow defaults: it now passes `--sdk-dir /home/nicolas/Android/Sdk` automatically, errors fast if that default SDK path does not exist, and still allows explicit override via `--sdk-dir /path/to/Android/Sdk`.
- 2026-02-16: Added root helper script `build_android.sh` for the standard Android sideload path. It invokes `android/scripts/build_and_sync_apk.sh` with defaults `--app tap --sync-dir ~/Sync` so running `./build_android.sh` directly builds and writes the APK into the sync folder.
- 2026-02-16: Improved Android writer state feedback in both app source copies (`android/InkiNfcTapToBook_androidstudio/` and `android/inki_nfc_tap_to_book/`):
  - Added explicit UI state line: `Ready - Tap to write` -> `Writing...` -> `Done` or `No Success - Try again!`.
  - Added write verification by comparing read-back payload bytes against the written request; mismatches now surface as failure (`No Success - Try again!`) instead of log-only.
  - Added haptic feedback for success and failure states.
- 2026-02-16: Updated wake/power policy for battery test flow:
  - ST25 wake GPO configuration changed to `GPO_ENABLE + RF_WRITE` only (removed `FIELD_CHANGE`) to avoid wake on mere field presence.
  - Boot request handling now powers down immediately when no valid `INKI` request is available and RF field is already OFF.
  - If RF field is still ON and boot request is not yet valid, firmware defers final decision to runtime; if the field then drops without a valid `INKI` command being seen, firmware powers down immediately.
- 2026-02-15: Remapped opcode `0x12` (fast command) to the same onboard power LED channel as `0x11` on current single-LED hardware. Firmware now applies `0x11` as LED1 slow blink and `0x12` as LED1 fast blink; no visible behavior depends on `GP15` anymore for this test flow. Updated command log text and README/firmware README mappings accordingly.
- 2026-02-15: Updated latch timeout default to 10 s for tap tests: `NFC_AUTO_POWER_OFF_MS` changed from `5000` to `10000` in firmware defaults (`firmware/src/main.c`, `firmware/CMakeLists.txt`), build script default/export (`firmware/scripts/build.sh`), and docs (`README.md`, `firmware/README.md`).
- 2026-02-15: Clarified LED command visibility from live logs (before same-day single-LED remap of `0x12`):
  - `opcode=0x11` (LED1 slow) drives the power LED path and is visible on current hardware.
  - `opcode=0x12` (LED2 fast) is successfully parsed/applied but drives `NFC_STATUS_LED_PIN` (`GP15`), which is not populated/routed to an onboard LED on this PCB revision.
  - Blink timing constants are toggle intervals: slow `800 ms`, fast `150 ms`. Equivalent full on-off blink cycle frequencies are `0.625 Hz` (slow, `1.6 s` period) and `3.33 Hz` (fast, `0.3 s` period).
- 2026-02-15: Fixed command-apply gap observed in latch wake flow when request is written during phone tap but RF field is already OFF by the time runtime loop starts. Root cause: command processing previously happened only in RF-ON runtime branch. New behavior: firmware now performs an explicit boot-stage stored-request check after startup diagnostics and runtime init. If a valid `INKI` request is present and RF field is OFF, firmware applies opcode immediately, re-arms auto power-off, and clears request right away; if RF is ON, it logs defer-to-live-loop and keeps existing runtime behavior. Added flow-specific logs (`Boot request ...`) and clarified command routing logs (`LED1` power LED, `LED2` on `GPIO15`).
- 2026-02-15: Added Android sideload workflow helper script `android/scripts/build_and_sync_apk.sh` to standardize "build on PC + copy APK to sync folder + install on phone" flow without requiring USB debugging. Script supports `--app tap|hello`, defaults to `tap` and `~/Sync`, auto-detects Android Studio JBR and `$HOME/Android/Sdk` when available, and writes `local.properties` for the selected Android Studio project when SDK path is known.
- 2026-02-15: Implemented NFC-command test flow + timed power-off for latch experiments:
  - Firmware now supports command opcodes in the 16-byte `INKI` payload: `0x11` (LED1 slow blink), `0x12` (LED2 fast blink), plus legacy `0x01` mapped to LED1 slow.
  - Added runtime-configurable auto power-off by releasing the latch pin after timeout (`NFC_AUTO_POWER_OFF_MS`, default `5000` ms). Timeout is armed at runtime start and re-armed when a supported command is applied.
  - Added configurable blink periods (`NFC_LED_SLOW_PERIOD_MS`, `NFC_LED_FAST_PERIOD_MS`) and command-driven LED mode handling in `firmware/src/main.c`.
  - Preserved existing ST25 robustness behavior: request parsing/validation and deferred clear after `RF field` transitions `OFF`.
  - Android app updated (both source copies: `android/inki_nfc_tap_to_book/` and `android/InkiNfcTapToBook_androidstudio/`) with two command buttons (`LED1 Slow`, `LED2 Fast`) that select opcode written into the `INKI` request payload.
- 2026-02-15: Captured current PCB wake/latch wiring from live KiCad board file (`pcb/NFC_harness_V0/NFC_harness_V0.kicad_pcb`) for this repo revision:
  - ST25 `GPO_(OPEN_DRAIN)` is net `38`; it directly drives `Q1` gate (P-MOS high-side switch), with pull-up `R4=1M` to `Vbatt` and RC pulse-stretch via `C5=22n` to GND.
  - `GPIO28` (`A1` pad 34, net `39`) drives `Q2` base through `R5=100k`; `R9=50k` pulls base to GND; `Q2` collector also sinks net `38`. This is the MCU latch-hold path.
  - `Q1` source is `Vbatt` and drain feeds Pico `VSYS` (`A1` pad 39), so pulling net `38` low turns board power on.
  - `GPIO19` is unconnected on this PCB revision (wake path is hardware-latch based, not MCU input sampled).
  - LED note: no dedicated D1/D2 footprints are present in this revision; command `LED1` uses firmware power LED output (Pico/Pico W onboard path), and `LED2` uses `NFC_STATUS_LED_PIN` (default GP15), which is currently unconnected in the captured PCB netlist unless wired externally.
- 2026-02-15: Real-hardware validation on battery-powered latch path succeeded: NFC event can power on the circuit and firmware latches power via `GPIO28` (`NFC_POWER_HOLD_PIN`). Boot logs confirm early latch assertion and stable ST25 communication after wake.
- 2026-02-15: Adjusted startup diagnostics to avoid false-negative bring-up while wake path is proven on hardware: wake-GPO boot configuration remains enabled/retried, but `SELFTEST` now treats `wake_gpo` as non-fatal by default (`NFC_WAKE_GPO_SELFTEST_STRICT=0`). Strict mode can be re-enabled with `NFC_WAKE_GPO_SELFTEST_STRICT=1`.
- 2026-02-15: Added NFC wake-latch support for the new power path: firmware now asserts `NFC_POWER_HOLD_PIN` (default `GP28`) immediately at boot to keep external power-latch circuitry engaged, and configures ST25 GPO wake behavior at startup (`NFC_ENABLE_WAKE_GPO_CONFIG=1`): pulse length `IT_TIME=0` (~302 us), sources `GPO_ENABLE + FIELD_CHANGE + RF_WRITE`, plus configuration readback logging. This directly supports "NFC event powers on board, MCU latches power via GPIO28".
- 2026-02-15: Set Pico W as the default firmware build target for this repo (`PICO_BOARD=pico_w` in `firmware/CMakeLists.txt` and default in `firmware/scripts/build.sh`). This prevents recurring accidental plain-Pico builds where onboard LED behavior differs (GP25 log path) and keeps generated `firmware/build/CMakeCache.txt` aligned with expected hardware.
- 2026-02-15: Added board-aware power LED handling for Pico vs Pico W. Firmware now uses RP2040 GPIO (`NFC_POWER_LED_PIN`, default `GP25`) on Pico, and CYW43 LED control (`CYW43_WL_GPIO_LED_PIN`) when built with `PICO_BOARD=pico_w` (linking `pico_cyw43_arch_none`). This fixes the "Power LED ON" log with no visible LED on Pico W builds that were previously driving non-existent `GP25` LED hardware.
- 2026-02-15: Added dedicated firmware power-indicator LED support (`NFC_POWER_LED_PIN`, default `GP25` for Pico onboard LED). Boot now explicitly sets this LED ON (`Power LED: GP25 -> ON`) so board-power state is visible independently from the existing RF/status LED behavior.
- 2026-02-15: Updated firmware default I2C pin mapping to match the Pico harness PCB wiring (`NFC_I2C_SDA_PIN=20`, `NFC_I2C_SCL_PIN=21`) in both `firmware/CMakeLists.txt` and fallback defines in `firmware/src/main.c`. This avoids fresh builds coming up on legacy `GP4/GP5` and failing ST25 I2C probe (`0x53/0x57` NACK) unless overrides are supplied.
- 2026-02-10: Added explicit boot log for configured GPO pulse duration (`IT_TIME`) so startup diagnostics now print both raw enum value and approximate pulse width in microseconds (e.g. `DYN GPO IT pulse: IT_TIME=0 (~302 us)`). This clarifies that `DYN GPO` itself is dynamic status, while pulse length comes from static GPO2 configuration.
- 2026-02-10: Added short inline hints to startup dynamic-status logs in firmware (`DYN I2C session`, `DYN EH`, `DYN RF field`, `DYN VCC`, `DYN RF mngt`, `DYN IT`, `DYN GPO`, `DYN mailbox`, `DYN mailbox length`) so boot diagnostics are self-explanatory without changing behavior.
- 2026-02-10: Reduced runtime serial noise during RF/V_EH testing by removing per-edge `RF/EH state: ...` logging from the steady-state loop; kept startup diagnostics unchanged (`DYN EH`, `DYN GPO`, EH test-mode snapshot, and `SELFTEST` summary still printed at boot).
- 2026-02-10: Detailed protocol note for integrating ST25DV wake into the inki-style gate-latch architecture (RTC/user wake baseline + added NFC wake path):
  - Clarified baseline: inki is currently **not** using NFC for wake; existing wake is RTC (`DS3231 ~INT`) or user pushbutton pulling the shared gate node low, then Pico firmware latches power via `Q2`/`GP22`.
  - ST25DV04KC-IE (`SO8`, open-drain GPO) behavior relevant to this integration:
    - `GPO` requires a pull-up supply and signals interrupts by pulling low (open-drain sink).
    - `GPO` source/pulse are configurable through `GPO1/GPO2` static registers (driver APIs `ST25DV_ConfigureGPO`, `ST25DV_WriteITPulse`).
    - For wake testing, target configuration is `FIELD_CHANGE` (and optionally `RF_WRITE`) with longest pulse (`IT_TIME=000`, about `301 us` nominal).
    - Datasheet caveat for `RF ON + VCC OFF`: RF events can be reflected on GPO if pull-up is powered, but `FIELD_CHANGE` on RF field falling is not reported when VCC is off.
  - `V_EH` observation from bench discussion: measured around `30 mV` when phone is tapped in current setup, therefore not a viable direct wake source in the current antenna/coupling/tuning state.
  - Shared-node RC caution and decision:
    - RC on the shared OR node (Q1 gate / RTC INT / manual trigger node) affects all wake channels.
    - If that RC is moved away from the shared node, a **dedicated GPO pulse-stretch branch** must be added because raw GPO pulses are short.
  - Low-leakage stretcher branch proposed for trials (compatible with sub-`5 uA` standby target if implemented carefully):
    - `GPO_STR` node: pull-up `1M..2.2M` to always-on rail + capacitor `22..47 nF` to GND.
    - Schottky isolation diode from shared gate node to `GPO_STR` (`anode=shared gate`, `cathode=GPO_STR`) so GPO pulses pull gate low while isolating other OR sources.
    - Keep wake branch isolated from switched `3V3`/MCU GPIO rails to avoid back-power/leakage when Pico is off.
  - Validation protocol before adopting in hardware:
    - Scope `GPO raw`, `GPO_STR`, shared gate node, and `VSYS` during phone tap.
    - Confirm Pico boot + latch success rate across phones/orientations/coupling.
    - Verify off-state current remains at baseline (order of a few microamps).
    - Confirm no unintended I2C back-power paths when ST25 VCC is off and RF is present.
- 2026-02-10: Hardened RF request transaction behavior to avoid partial-write races during phone taps: firmware now validates full `INKI` payload semantics (not magic-only), ignores incomplete/invalid frames (e.g. `INKI` + zeros), and defers clearing request memory until `RF field` transitions `OFF` to reduce RF-session interference with phone readback.
- 2026-02-10: Android tap-to-book app write path updated for better transactional behavior on marginal coupling: transceive retries added, NFC-V timeout increased, post-write readback downgraded to warning when tag is lost after a successful write, and request blocks are now written in reverse order so the `INKI` magic block is written last.
- 2026-02-10: Reduced runtime RF-field poll noise from transient I2C read glitches: RF field status reads now use a short bounded retry window, first miss logs as transient warning, persistent misses escalate only after consecutive failures, and successful read logs a recovery message. This keeps normal tap interactions clean while still surfacing real faults.
- 2026-02-10: Switched runtime (post-startup) logging to RF-event-driven behavior: no continuous 1 Hz request dump while idle, RF field ON/OFF edge logs, request-slot reads only while RF field is ON, and action-focused logs only when content changes (`INKI` parse/clear, empty slot, non-INKI payload ignored, and read/clear error + recovery events).
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
