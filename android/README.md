# Android (ISO15693 / NFC‑V)

Android pieces are optional for the Pico harness; use them only if you want to exercise phone→tag writes. The minimal app writes a tiny “booking request” into an **ISO15693 / NFC‑V (Type 5)** tag (e.g. **ST25DV04KC**) and reads it back for verification.

## What you can (and can’t) test without hardware
- You **need a physical NFC‑V / ISO15693 tag** to exercise the actual read/write logic.
- Phones generally **cannot emulate ISO15693 tags** (Android HCE targets ISO14443 types), so there’s no realistic “tag emulator” substitute.

## Folders (choose based on need)
- `android/inki_nfc_tap_to_book/` (drop‑in)
  - Minimal source set (Manifest + `MainActivity.kt` + layout + strings + tech filter).
  - Uses `android.nfc.tech.NfcV` with raw ISO15693 commands: Get System Info `0x2B`, Read Single Block `0x20`, Write Single Block `0x21`.
  - UI includes command buttons (`LED1 Slow`, `LED2 Fast`) that select the opcode written into the 16-byte `INKI` request.
  - UI result feedback uses a large auto-hiding popup: red `No Success - Try again!` on failure, green `Success! Slow/Fast` on verified write/readback success.
  - Request payload spec: `android/inki_nfc_tap_to_book/REQUEST_FORMAT.md` (16‑byte `INKI` payload, little‑endian fields).
- `android/InkiNfcTapToBook_androidstudio/` (full project)
  - Android Studio project built from “Empty Views Activity”; sources copied from the drop‑in.
  - Tweaks for sideloading: `minSdk=21`, `uses-feature android.hardware.nfc required="false"` so it installs on non‑NFC devices (app still needs NFC to function).
- `android/InkiHello_androidstudio/`
  - Tiny “hello” app used only to validate the sideload workflow/Play Protect.

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
- `android/InkiHello_androidstudio/app/build/outputs/apk/debug/app-debug.apk`

## Build + Sync Script (recommended for sideload flow)
Use the repo script to build and copy APK into your sync folder:

```bash
# from repo root
./android/scripts/build_and_sync_apk.sh --app tap
```

Defaults:
- app: `tap` (`InkiNfcTapToBook_androidstudio`)
- sync folder: `~/Sync`
- output filename: `inki_nfc_tap_to_book-debug.apk`

Examples:
```bash
# build + copy hello app
./android/scripts/build_and_sync_apk.sh --app hello

# build only (no copy)
./android/scripts/build_and_sync_apk.sh --app tap --no-copy

# custom sync folder
./android/scripts/build_and_sync_apk.sh --app tap --sync-dir ~/Syncthing/apk

# explicit SDK path override
./android/scripts/build_and_sync_apk.sh --app tap --sdk-dir ~/Android/Sdk
```

The script auto-detects:
- `JAVA_HOME=/snap/android-studio/current/jbr` when available
- `ANDROID_SDK_ROOT` from, in order: `--sdk-dir`, env var, `$HOME/Android/Sdk`, `/snap/android-studio/current/android-sdk`, `/snap/android-studio/common/android-sdk`
- and writes `local.properties` in the selected Android Studio project.

If SDK is missing, the script exits early with setup instructions instead of failing later in Gradle.

## Install without USB cable (Syncthing)
- Copy the debug APK into a Syncthing-synced folder (e.g. `~/Sync/`).
- On the phone, open the APK from the synced folder and install.
- Some devices trigger **Play Protect** scanning for sideloaded apps (especially those requesting NFC); installation can still proceed after the scan.

Convenience copies used during travel testing:
- `~/Sync/inki_hello-debug.apk`
- `~/Sync/inki_nfc_tap_to_book-debug.apk`

## Quick start (sideload path)
1) Build: run the CLI build above (or Android Studio → Build APK).  
2) Copy `app-debug.apk` to your phone (e.g., Syncthing).  
3) Open the APK; Play Protect may scan NFC apps—proceed after scan.  
4) Open the app and tap a physical ISO15693/NFC‑V tag (write happens automatically).  

## Not needed for Pico-only tests
If you’re only working on the Pico harness, you can ignore the Android folder; firmware tests don’t depend on it.***
