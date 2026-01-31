# context – NFC_exploration

Sandbox repo to explore **ST25DV04KC (ISO15693 / NFC‑V)** “tap‑to‑trigger / tap‑to‑book” flows for inki.

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
- Firmware (currently minimal):
  - `firmware/src/main.c` (Pico/RP2040 I²C scan stub)
- KiCad harness starter:
  - `pcb/NFC_harness_V0/`
- ST25DV I²C driver (vendored):
  - `firmware/third_party/st25dv/` (git subtree of `STMicroelectronics/stm32-st25dv`)

## Android app: `InkiNfcTapToBook`

### What it does (current MVP)
- Uses Android reader mode + `NfcV` to talk ISO15693 (NFC‑V) tags.
- UI:
  - `Clear log` clears the on‑screen log view.
  - `Write on tap` controls whether the app **writes** a test/request payload whenever a tag is detected.
- Tag interaction (high‑level):
  - Reads basic tag info (via ISO15693 “Get System Info” when possible).
  - Reads/writes blocks using raw `NfcV.transceive()` ISO15693 commands (read single block / write single block).

### Where the NFC code lives
- Main activity: `android/InkiNfcTapToBook_androidstudio/app/src/main/java/.../MainActivity.kt`
- Drop‑in sources: `android/inki_nfc_tap_to_book/` (same logic, easier to copy between projects)

### Build + sideload without USB cable
Build (Android Studio: “Build APK(s)”, or CLI):
```bash
cd android/InkiNfcTapToBook_androidstudio
JAVA_HOME=/snap/android-studio/current/jbr \
ANDROID_SDK_ROOT="$HOME/Android/Sdk" \
./gradlew :app:assembleDebug
```

APK output:
- `android/InkiNfcTapToBook_androidstudio/app/build/outputs/apk/debug/app-debug.apk`
- `android/InkiHello_androidstudio/app/build/outputs/apk/debug/app-debug.apk`

Sideload workflow used while traveling:
- copy to `~/Sync/` (Syncthing)
  - `~/Sync/inki_nfc_tap_to_book-debug.apk`
  - `~/Sync/inki_hello-debug.apk`
- install on phone via file manager
  - “Hello” installs without warnings
  - NFC app can trigger a Play Protect scan, but installs after the scan

## ST25DV04KC: ultra‑low power notes (AN5733 + datasheet)

### Power strategy in one sentence
Power ST25DV VCC only when the MCU needs I²C; keep RF functionality when VCC is off; use GPO/field events to wake MCU if desired.

### Key points pulled from AN5733
- VCC can be powered directly from a MCU GPIO (AN5733 notes ST25DV current is <200 µA at 1.8 V).
- Best‑case sequence uses **LPD** (only in 10‑ball/12‑pin packages) before removing VCC to avoid rebooting during an RF EEPROM programming cycle.
- If using an **8‑pin** package (no LPD):
  - slow VCC rise/fall (AN5733 suggests adding **≥10 µF** on VCC to reduce reboot risk)
  - keep **GPO pulled up permanently** (open‑drain output) so RF interrupts can still be signaled

### I²C addressing (datasheet, default)
ST25DV uses different “device select” values for different memories:
- `A6h/A7h`: user memory + dynamic regs + FTM mailbox (write/read)
- `AEh/AFh`: system memory (write/read)

On a normal 7‑bit I²C API, this typically means:
- `A6h >> 1 = 0x53` (write), `A7h >> 1 = 0x53` (read)
- `AEh >> 1 = 0x57` (write), `AFh >> 1 = 0x57` (read)

## Can we reuse `stm32-st25dv` (ST25DV driver) with Pico firmware?
Short version: yes — by providing a small bus-IO adapter layer (I²C read/write + tick + ready) for the driver.

### Why it didn’t fit the earlier ATtiny idea
- ATtiny202 has ~2 KB flash; ST’s driver is a fairly complete feature driver (register layer + high‑level API), so it won’t fit without heavy pruning.

### What is still useful from that repo
- `st25dv_reg.h`: register addresses and bitfield masks (great for implementing a minimal subset)
- `st25dv.c/h`: shows how ST expects access patterns to work (two I²C “device selects”, dynamic regs at `0x2000+`, mailbox region, etc.)

### What to do instead
- Use Pico/RP2040 for the harness and either:
  - implement a tiny “just what we need” ST25DV I²C layer, or
  - port selected parts of ST’s driver with Pico SDK I²C hooks.

## Firmware status (Pico/RP2040)
- `firmware/src/main.c` reads the last 16 bytes of ST25DV user memory (Android “INKI” request payload), prints it, and clears it when present.
- Build (Pico SDK):
```bash
cd firmware
./scripts/build.sh
```

## Next steps
- Define the harness pinout (I²C pins, optional status LED pin) and then implement the “minimum viable” ST25DV I²C reads/writes for the request blocks.
- Add a first “ST25 explore” schematic to `pcb/NFC_harness_V0/` based on AN5733 + ST25DV04KC datasheet (VCC strategy, GPO pull‑ups, decoupling, antenna matching).
- Once hardware is available: validate on a real tag (RF writes, wake behavior, timing, and robustness).

## Log
- 2026-01-25: Repo scaffolded (AVR build/flash scripts + KiCad/CAM template copied and renamed to `NFC_harness_V0`).
- 2026-01-25: Added Android NFC‑V writer app sources under `android/inki_nfc_tap_to_book/` and created Android Studio projects for sideload tests.
- 2026-01-25: Travel notes: no ISO15693 tag available. Hello APK installs cleanly; NFC APK can trigger Play Protect scan but installs after scan.
- 2026-01-27: Added notes from AN5733 + ST25DV04KC datasheet and reviewed ST’s `stm32-st25dv` driver for reuse (good for Pico, too big for ATtiny202 as‑is).
- 2026-01-28: Switched harness firmware to Pico/RP2040 (Pico SDK + `firmware` workflow) and removed the AVR/ATtiny build/flash scaffolding.
- 2026-01-28: Vendored ST’s `stm32-st25dv` driver as a git subtree and wired it into the Pico harness firmware.
