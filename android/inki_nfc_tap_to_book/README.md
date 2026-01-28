# inki NFC tap-to-book (Android MVP)

This folder contains **drop-in Android sources** for a minimal app that can talk to **ISO15693 / NFC-V** tags (e.g. **ST25DV04KC**) using `android.nfc.tech.NfcV`.

The MVP behavior is:
- when a tag is tapped, the app optionally writes a small **16-byte booking request** into the **last N blocks** of the tag’s user EEPROM (defaults to “last 4 blocks” when the tag reports block count)
- then reads back the written blocks to verify

## How to use

1) In Android Studio: **New Project → Empty Views Activity**
- Language: **Kotlin**
- Min SDK: **21+** is fine
- Package name (recommended): `de.c0de111.inki.nfc.taptobook`

2) Replace the generated files with the ones in this folder:
- `app/src/main/AndroidManifest.xml`
- `app/src/main/java/de/c0de111/inki/nfc/taptobook/MainActivity.kt`
- `app/src/main/res/layout/activity_main.xml`
- `app/src/main/res/xml/nfc_tech_filter.xml`

3) Build + install on your phone.

4) Open the app and tap your ISO15693 / NFC-V tag.
- Enable **“Write on tap”** in the app before tapping if you want it to write.

## Request format
See `REQUEST_FORMAT.md`.

## Notes / gotchas
- UID byte order can be confusing; this code uses the UID as returned by `Tag.getId()` for addressed-mode commands.
- For ST25DV04KC (4 kbit = 512 bytes), “last 4 blocks” typically means blocks **124–127** (4 bytes per block).

## Testing without tag
- You need a physical **ISO15693 / NFC‑V / Type 5** tag to exercise the code (e.g. ST25DV04KC, ICODE, etc.).
- Phones generally **cannot emulate** an ISO15693 tag (Android HCE is for ISO14443 types), so there’s no realistic “simulator-only” test.

## Installing without USB cable (travel)
- Build an APK in Android Studio (`Build → Build Bundle(s)/APK(s) → Build APK(s)`).
- Sideload it to the phone (e.g. via Syncthing). Some devices trigger **Play Protect** scanning for sideloaded NFC apps; after scanning you can usually proceed to install.

## What you should see
- App UI: “Write on tap” checkbox + log view.
- Without a NFC‑V tag, tapping random NFC tags will usually print “No NfcV on this tag”.
