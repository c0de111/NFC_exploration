# Notes for Agents Working on inki

This repo contains firmware, hardware (KiCad) and docs for the inki/eSign device. This file records practical notes and recent hardware changes so assistants don’t miss context while editing code or schematics.

Process notes (keep private):
- Append a concise log of assistant chats and mirror decisions in `context.md` (not on the public allowlist).
- If adjusting the mirror, update `context.md` with what changed and why; keep public history intact (no force-push).
- After each chat session, record relevant content/results in `context.md` with enough detail to resume work later.

## Hardware maturity naming (private vs public)
- Use `hardware/circuit/` for the stable/public baseline.
- Use explicit level tracks for private variants:
  - Level II private integration track: `hardware/circuit_l2/`
  - Level III product/commercial candidate track: `hardware/circuit_l3_product/`
- Private-track hardware may include specialized parts/processes that are not intended for home assembly.
- Private-track hardware may also serve as a candidate base for a future commercial product line.
- Even if electrically similar to the public board, integration/layout optimization can remain private until explicitly promoted.
- Promotion rule: if/when a variant should become public, move/copy the finalized design into `hardware/circuit/`, then update docs and `context.md`, and adjust `.public-allowlist`.
- Keep private hardware paths explicitly excluded from `.public-allowlist` (for example `!hardware/circuit/pushbuttons/`, `!hardware/circuit_l2/`, and `!hardware/circuit_l3_product/`).

## Hardware: NFC / ISO15693 Tag
- For design and test notes on ST25DV04KC NFC wake, see `docs/nfc_st25dv04kc.md` (private, not published).

## Don’ts (to preserve µA budget)
- Do not tie any resistor from button raw/base nodes to `GATE` without isolating the MCU side; this creates a DC path into the unpowered 3V3 rail.
- Do not place the `GATE` bias diode or any “wake” element on the MCU‑side node; always use the raw switch node.

## Quick test checklist
- Off‑state current near baseline (DS3231 + passives). Buttons not pressed: no rise beyond ~2–3 µA total.
- Any single/multiple button press wakes the unit; only the pressed buttons read LOW in firmware.
- Net highlight in KiCad:
  - `GATE` includes Q1 gate, DS3231 `~INT`, J2, and the anodes of all `DBIAS` diodes.
  - `PBx_RAW` includes the cathodes of `DBIAS` and `DISO` and the switch contact, but not the MCU pin.
  - `PBx_GPIO` includes the anode of `DISO`, the 1 k/100 nF, and the Pico pin.

## Work cadence directive
- Before each small implementation step, show the exact source files/sections you plan to change and briefly explain scope/approach. Do not start editing until this is shown and agreed.
- After larger refactors or cross-use-case changes, build-test all firmware use cases (`--seatsurfing`, `--historian`, `--homematic`, `--weathermap`) before declaring the repo ready.
- Build/test checks are user-owned in this repo flow; do not run checks unless the user explicitly asks.

## Git workflow directive
- Use an nvie/Gitflow-style branch model unless explicitly decided otherwise: `main` for stable/release history, `develop` as the integration branch, and `feature/*` branches for isolated work.
- Start new feature work from `develop` (for example firmware features, `inki-monitor`, or hardware rework tracks like PCB L2) instead of working directly on `develop`.
- Merge completed `feature/*` branches back into `develop` with a non-fast-forward merge (`git merge --no-ff ...`) to preserve the feature boundary/history.
- For longer-lived feature branches, periodically sync by merging `develop` into the feature branch (`git merge develop`) to keep final merge conflicts smaller.
- Merge execution is user-owned: the assistant provides exact merge commands and guidance, and does not run merge/push commands unless the user explicitly asks.

## Code display directive
- When showing code snippets, use color cues: green for new code, blue for proposed, red for removed, orange for under-discussion/questionable pieces.
- When reporting build/test outcomes, use the same colors: 🟢 success/plan, 🟥 errors, 🔵 proposed, 🟠 warnings/uncertain.
- When presenting tabular information in chat responses, use ASCII-art tables (monospace `+`, `-`, `|` borders) instead of Markdown tables.
- When suggesting shell commands for the user to run, prefer a single-line copy/paste-ready command unless the user explicitly asks for a multi-line version.
- For tracking future tasks/ideas/TODOs, always present and update a concise ASCII-art table with columns: `ID`, `Future Topic`, `Status`, `Source`.
- Future-project tables must be complete (include all currently open tracked future tasks/ideas/TODOs, not only a subset from the current chat).
- Future-project tables in chat must use one line per project (no wrapped multi-line rows).
- The canonical source of truth is the `## Future Projects (Canonical)` table near the beginning of `context.md`; chat responses must mirror IDs/topics/status from that table.
- In chat future-project tables, keep `Source` compact (short labels, no date strings); keep detailed dated source pointers in the canonical `context.md` table.
