# AGENTS – working notes for future assistants

Purpose: capture repo-specific directives agreed in chat so new sessions stay aligned.

## Scope & priorities
- Focus is Pico/RP2040 harness with ST25DV04KC; Android apps are secondary and can be ignored unless requested.
- Keep firmware pico-only; no other MCU targets planned.

## Docs
- `README.md` is overview only (no history). Links to `docs/context.md` for all detailed notes, history, solved problems, and logs.
- `docs/context.md` is the single living log; add new findings there. Do not recreate root-level context/codex files.

## Firmware layout & build
- Firmware lives under `firmware/` with `src/`, `scripts/`, `cmake/`, `third_party/st25dv/`, `build/` (gitignored).
- Build from repo root via `make firmware`; clean via `make clean` (removes only `firmware/build/`).
- Flash via SWD: `make firmware-flash`. Drag-and-drop uses the UF2 in `firmware/build/`.
- Picotool is disabled by default (`PICO_NO_PICOTOOL=ON`, `PICO_NO_UF2=ON`). UF2 generation is picotool-free: `firmware/scripts/build.sh` downloads/caches `uf2conv.py` + `uf2families.json` and converts `nfc_harness.bin` → `nfc_harness.uf2`.
- If picotool/signing is desired, reconfigure CMake with `-DPICO_NO_PICOTOOL=OFF -DPICO_NO_UF2=OFF` (not default).

## Tooling notes
- `PICO_SDK_PATH` expected (defaults to `$HOME/pico/pico-sdk`).
- `build.sh --clean` must only delete `firmware/build/` and exit (no configure/build).

## Repo structure expectations
- Root `Makefile` exposes: `firmware`, `firmware-clean`, `firmware-flash`, `clean` (alias to firmware-clean).
- Android folder can stay untouched unless explicitly needed.
- Hardware: KiCad project in `pcb/NFC_harness_V0/`; shared libs in `pcb/components/` and `pcb/project-libraries/` (submodule). Add docs there if editing PCB flow.

## Do / Don’t
- Do keep history and detailed decisions in `docs/context.md` only.
- Don’t reintroduce `context.md` or `codex.md` at repo root.
- Avoid enabling picotool unless explicitly requested.
- Keep comments minimal and ASCII; follow existing style.
- Write git commit messages as exactly one sentence focused on architecture and implementation progress (avoid parameter/value minutiae).

## Quick commands
- Build: `make firmware`
- Clean: `make clean`
- Flash (SWD): `make firmware-flash`
- UF2 output path: `firmware/build/nfc_harness.uf2`
