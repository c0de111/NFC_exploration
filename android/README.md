# Android (ISO15693 / NFC‑V)

This repo includes a minimal Android app to write a tiny “booking request” into an **ISO15693 / NFC‑V (Type 5)** tag (e.g. **ST25DV04KC**) and read it back for verification.

## What you can (and can’t) test without hardware
- You **need a physical NFC‑V / ISO15693 tag** to exercise the actual read/write logic.
- Phones generally **cannot emulate ISO15693 tags** (Android HCE targets ISO14443 types), so there’s no realistic “tag emulator” substitute.

## Folders
- `android/inki_nfc_tap_to_book/`
  - “Drop‑in” sources (Manifest + `MainActivity.kt` + layout + tech filter).
  - The app uses `android.nfc.tech.NfcV` and raw ISO15693 commands:
    - Get System Info `0x2B`
    - Read Single Block `0x20`
    - Write Single Block `0x21`
  - Request payload spec lives in `android/inki_nfc_tap_to_book/REQUEST_FORMAT.md`.

- `android/InkiNfcTapToBook_androidstudio/`
  - Android Studio project (created via “Empty Views Activity”) that the drop‑in sources were copied into.
  - Tweaks made for travel/sideload testing:
    - `minSdk` lowered to 21
    - NFC feature set to `required="false"` (APK can install even on devices without NFC; app obviously still needs NFC to do anything)

- `android/InkiHello_androidstudio/`
  - Super simple “Hello (install test)” app used to validate the sideload workflow.

## Build (CLI)
If Gradle complains about the system Java toolchain, use Android Studio’s bundled JDK:

```bash
cd android/InkiNfcTapToBook_androidstudio
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export JAVA_HOME="/snap/android-studio/current/jbr"
export PATH="$JAVA_HOME/bin:$PATH"
./gradlew :app:assembleDebug --no-daemon
```

APK output:
- `android/InkiNfcTapToBook_androidstudio/app/build/outputs/apk/debug/app-debug.apk`

## Install without USB cable (Syncthing)
- Copy the debug APK into a Syncthing-synced folder (e.g. `~/Sync/`).
- On the phone, open the APK from the synced folder and install.
- Some devices trigger **Play Protect** scanning for sideloaded apps (especially those requesting NFC); installation can still proceed after the scan.

Convenience copies used during travel testing:
- `~/Sync/inki_hello-debug.apk`
- `~/Sync/inki_nfc_tap_to_book-debug.apk`
