#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: ./android/scripts/build_and_sync_apk.sh [--app tap|hello] [--sdk-dir DIR] [--sync-dir DIR] [--no-copy]

Builds debug APK via Gradle and copies it to a sync folder for phone sideload.

Options:
  --app tap|hello   App to build (default: tap)
  --sdk-dir DIR     Android SDK path override (same as ANDROID_SDK_ROOT)
  --sync-dir DIR    Destination folder for copied APK (default: $HOME/Sync)
  --no-copy         Build only, do not copy
  -h, --help        Show this help

Environment:
  ANDROID_SDK_ROOT  Android SDK path (auto-detected as $HOME/Android/Sdk if present)
  JAVA_HOME         JDK path (auto-detected as /snap/android-studio/current/jbr if present)
EOF
}

APP="tap"
SYNC_DIR="${HOME}/Sync"
DO_COPY=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --app)
      [[ $# -ge 2 ]] || { echo "Missing value for --app" >&2; exit 2; }
      APP="$2"
      shift 2
      ;;
    --sync-dir)
      [[ $# -ge 2 ]] || { echo "Missing value for --sync-dir" >&2; exit 2; }
      SYNC_DIR="$2"
      shift 2
      ;;
    --sdk-dir)
      [[ $# -ge 2 ]] || { echo "Missing value for --sdk-dir" >&2; exit 2; }
      export ANDROID_SDK_ROOT="$2"
      shift 2
      ;;
    --no-copy)
      DO_COPY=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ANDROID_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

case "$APP" in
  tap)
    PROJECT_DIR="${ANDROID_DIR}/InkiNfcTapToBook_androidstudio"
    APK_REL="app/build/outputs/apk/debug/app-debug.apk"
    APK_NAME="inki_nfc_tap_to_book-debug.apk"
    ;;
  hello)
    PROJECT_DIR="${ANDROID_DIR}/InkiHello_androidstudio"
    APK_REL="app/build/outputs/apk/debug/app-debug.apk"
    APK_NAME="inki_hello-debug.apk"
    ;;
  *)
    echo "Unsupported --app value: $APP (use tap or hello)" >&2
    exit 2
    ;;
esac

if [[ -z "${JAVA_HOME:-}" && -d "/snap/android-studio/current/jbr" ]]; then
  export JAVA_HOME="/snap/android-studio/current/jbr"
fi
if [[ -n "${JAVA_HOME:-}" ]]; then
  export PATH="${JAVA_HOME}/bin:${PATH}"
fi

if [[ -z "${ANDROID_SDK_ROOT:-}" && -d "${HOME}/Android/Sdk" ]]; then
  export ANDROID_SDK_ROOT="${HOME}/Android/Sdk"
fi

if [[ -z "${ANDROID_SDK_ROOT:-}" && -d "/snap/android-studio/current/android-sdk" ]]; then
  export ANDROID_SDK_ROOT="/snap/android-studio/current/android-sdk"
fi

if [[ -z "${ANDROID_SDK_ROOT:-}" && -d "/snap/android-studio/common/android-sdk" ]]; then
  export ANDROID_SDK_ROOT="/snap/android-studio/common/android-sdk"
fi

if [[ -z "${ANDROID_SDK_ROOT:-}" || ! -d "${ANDROID_SDK_ROOT}" ]]; then
  cat >&2 <<'EOF'
Android SDK not found.

Set one of:
  1) ANDROID_SDK_ROOT=/path/to/Android/Sdk
  2) --sdk-dir /path/to/Android/Sdk

Typical setup with Android Studio:
  Android Studio -> Settings -> Android SDK
  Install at least:
    - Android SDK Platform 36
    - Android SDK Build-Tools 36.x
    - Android SDK Platform-Tools
EOF
  exit 1
fi

LOCAL_PROPERTIES="${PROJECT_DIR}/local.properties"
ESCAPED_SDK_DIR="$(printf '%s' "${ANDROID_SDK_ROOT}" | sed 's/\\/\\\\/g')"
printf 'sdk.dir=%s\n' "${ESCAPED_SDK_DIR}" > "${LOCAL_PROPERTIES}"

echo "Project: ${PROJECT_DIR}"
echo "App: ${APP}"
echo "ANDROID_SDK_ROOT=${ANDROID_SDK_ROOT}"
if [[ -n "${JAVA_HOME:-}" ]]; then
  echo "JAVA_HOME=${JAVA_HOME}"
fi

(
  cd "${PROJECT_DIR}"
  ./gradlew :app:assembleDebug --no-daemon
)

APK_PATH="${PROJECT_DIR}/${APK_REL}"
if [[ ! -f "${APK_PATH}" ]]; then
  echo "APK not found after build: ${APK_PATH}" >&2
  exit 1
fi

echo "Built APK: ${APK_PATH}"

if [[ "${DO_COPY}" -eq 1 ]]; then
  mkdir -p "${SYNC_DIR}"
  DEST="${SYNC_DIR}/${APK_NAME}"
  cp "${APK_PATH}" "${DEST}"
  echo "Copied APK to: ${DEST}"
fi
