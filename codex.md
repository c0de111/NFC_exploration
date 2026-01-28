# codex – NFC exploration journal (historical)

New running notes live in `context.md`. This file is kept for early project history.

## Plan
- Bring up ST25DV04KC + ATtiny202 harness (power, I²C, antenna)
- Validate phone → EEPROM writes (NfcV)
- Implement MCU-side request parsing + clearing
- Measure timing and range with enclosure + ferrite

## Chat log
- 2026-01-25: Repo scaffolded from PlaygroundSentinel workflow: AVR build/flash scripts + KiCad/CAM template copied and renamed to NFC_harness_V0.

- 2026-01-25: Added minimal Android NFC-V writer app drop-in sources under `android/inki_nfc_tap_to_book/` (raw ISO15693 read/write + 16-byte request format).

- 2026-01-25: Built APKs for sideload testing via Syncthing: `android/InkiHello_androidstudio/` (hello app) → `~/Sync/inki_hello-debug.apk`; and updated `android/InkiNfcTapToBook_androidstudio/` (minSdk 21, NFC optional) → `~/Sync/inki_nfc_tap_to_book-debug.apk`.

- 2026-01-25: Travel notes: no ISO15693 tag available to test. Hello APK installs cleanly; NFC APK can trigger Play Protect scan but installs after scan. App currently logs only to its on-screen log (no Logcat/exports).
