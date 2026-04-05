#!/usr/bin/env bash
# Shared pcb2gcode runner. Picks the right board/profile combo and wraps logging + cleanup.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

# Locate pcb2gcode: prefer PATH, then a common local build under ~/github/pcb2gcode/pcb2gcode.
if [[ -z "${PCB2GCODE_BIN:-}" ]]; then
  if command -v pcb2gcode >/dev/null 2>&1; then
    PCB2GCODE_BIN="$(command -v pcb2gcode)"
  elif [[ -x "$HOME/github/pcb2gcode/pcb2gcode" ]]; then
    PCB2GCODE_BIN="$HOME/github/pcb2gcode/pcb2gcode"
  else
    PCB2GCODE_BIN="pcb2gcode"
  fi
fi

declare -a __millprojects_tmp_configs=()
declare -a __tmp_drill_files=()
cleanup_millprojects() {
  for tmp in "${__millprojects_tmp_configs[@]}"; do
    [[ -f "$tmp" ]] && rm -f "$tmp"
  done
  for tmp in "${__tmp_drill_files[@]}"; do
    [[ -f "$tmp" ]] && rm -f "$tmp"
  done
}
trap cleanup_millprojects EXIT

run_pcb2gcode_stage() {
  local label="$1"; shift
  local -a stage_cmd=("$@")
  local prefix="[$label] "
  local label_display="$label"
  printf "\n${bold}==> %s${reset}\n" "$label_display"
  if command -v stdbuf >/dev/null 2>&1; then
    if ! stdbuf -oL -eL "${stage_cmd[@]}" > >(sed "s/^/$prefix/") 2> >(sed "s/^/$prefix/" >&2); then
      printf "${red:-}Stage %s failed${reset:-}\n" "$label_display" >&2
      exit 1
    fi
  else
    if ! "${stage_cmd[@]}" > >(sed "s/^/$prefix/") 2> >(sed "s/^/$prefix/" >&2); then
      printf "${red:-}Stage %s failed${reset:-}\n" "$label_display" >&2
      exit 1
    fi
  fi
  printf "${green}<= %s${reset}\n" "$label_display"
}

usage() {
  cat <<'EOF'
Usage: pcb/tools/run_pcb2gcode.sh -b <board> [options] [-- pcb2gcode-flags]

Options:
  -b, --board NAME      Board directory under pcb/ (e.g., NFC_harness_V0). Required.
  -p, --profile NAME    Millproject profile to use (default: default).
  -c, --config PATH     Explicit millproject path (overrides --profile).
  --project NAME        Base filename for Gerbers (NAME-F_Cu.gbr, etc.) when overriding inputs.
  -f, --front FILE      Front Gerber file (relative names resolve inside the board's input directory).
  -e, --outline FILE    Edge cuts Gerber file.
  -d, --drill FILE      Drill file.
  -i, --input-dir DIR   Override the board's input directory (default cam/pcb2gcode/input).
  -o, --output-dir DIR  Override the board's output directory (default cam/pcb2gcode/output).
      --paste           Convenience flag: use the board's *-F_Paste.gbr as the front file
                        and skip outline/drill steps (unless you override them explicitly).
  -h, --help            Show this help and exit.

Anything after `--` (or any unrecognized positional arguments) is passed straight to pcb2gcode.

Example:
  ./run_pcb2gcode.sh -b LC_tank_coilcraft_pcb_cap
EOF
}

bail() {
  printf '%s\n' "$1" >&2
  exit 1
}

resolve_relative() {
  local path="$1"
  if [[ -z "$path" ]]; then
    printf ''
  elif [[ "$path" == /* || "$path" == ./* || "$path" == ../* ]]; then
    printf '%s' "$path"
  else
    printf '%s/%s' "$REPO_ROOT" "$path"
  fi
}

resolve_input_spec() {
  local spec="$1" input_dir="$2"
  if [[ -z "$spec" ]]; then
    printf ''
  elif [[ "$spec" == /* || "$spec" == ./* || "$spec" == ../* || "$spec" == *'/'* ]]; then
    printf '%s' "$spec"
  else
    printf '%s/%s' "$input_dir" "$spec"
  fi
}

BOARD_NAME=""
PROFILE_NAME="default"
CONFIG_OVERRIDE=""
PROJECT_OVERRIDE=""
INPUT_OVERRIDE=""
OUTPUT_OVERRIDE=""
front_override=""
outline_override=""
drill_override=""
paste_mode=false
declare -a passthrough_args=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    -b|--board)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      BOARD_NAME="$2"
      shift 2
      ;;
    -p|--profile)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      PROFILE_NAME="$2"
      shift 2
      ;;
    -c|--config)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      CONFIG_OVERRIDE="$2"
      shift 2
      ;;
    --project)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      PROJECT_OVERRIDE="$2"
      shift 2
      ;;
    -f|--front)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      front_override="$2"
      shift 2
      ;;
    -e|--outline)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      outline_override="$2"
      shift 2
      ;;
    -d|--drill)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      drill_override="$2"
      shift 2
      ;;
    -i|--input-dir)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      INPUT_OVERRIDE="$2"
      shift 2
      ;;
    -o|--output-dir)
      [[ $# -ge 2 ]] || bail "Option $1 requires an argument."
      OUTPUT_OVERRIDE="$2"
      shift 2
      ;;
    --paste)
      paste_mode=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      passthrough_args+=("$@")
      break
      ;;
    -*)
      bail "Unknown option: $1"
      ;;
    *)
      passthrough_args+=("$1")
      shift
      ;;
  esac
done

[[ -n "$BOARD_NAME" ]] || { usage >&2; bail "Please supply --board."; }

BOARD_DIR="pcb/$BOARD_NAME"
[[ -d "$BOARD_DIR" ]] || bail "Board directory $BOARD_DIR not found."

CAM_ROOT="$BOARD_DIR/cam/pcb2gcode"
[[ -d "$CAM_ROOT" ]] || bail "Expected CAM directory $CAM_ROOT."

PROFILE_DIR="$CAM_ROOT/profiles"

if [[ -n "$CONFIG_OVERRIDE" ]]; then
  CONFIG_PATH="$(resolve_relative "$CONFIG_OVERRIDE")"
else
  if [[ -f "$PROFILE_DIR/${PROFILE_NAME}.millprojects" ]]; then
    CONFIG_PATH="$PROFILE_DIR/${PROFILE_NAME}.millprojects"
  else
    CONFIG_PATH="$PROFILE_DIR/${PROFILE_NAME}.millproject"
  fi
fi

[[ -f "$CONFIG_PATH" ]] || bail "Millproject $CONFIG_PATH not found."

config_display_path="$CONFIG_PATH"
main_config_label="$PROFILE_NAME"
declare -a extra_configs=()
declare -a extra_labels=()

if [[ "$CONFIG_PATH" == *.millprojects ]]; then
  declare -a multi_names=()
  declare -a multi_files=()
  current_file=""
  while IFS= read -r line || [[ -n "$line" ]]; do
    if [[ "$line" =~ ^\[\[(.+)\]\] ]]; then
      current_file="$(mktemp)"
      __millprojects_tmp_configs+=("$current_file")
      multi_names+=("${BASH_REMATCH[1]}")
      multi_files+=("$current_file")
      continue
    fi
    [[ -n "$current_file" ]] || continue
    printf '%s\n' "$line" >>"$current_file"
  done < "$CONFIG_PATH"
  ((${#multi_files[@]} > 0)) || bail "Millprojects $CONFIG_PATH has no [[section]] blocks."
  if $paste_mode; then
    paste_idx=-1
    for i in "${!multi_names[@]}"; do
      name_lower="$(tr '[:upper:]' '[:lower:]' <<<"${multi_names[$i]}")"
      if [[ "$name_lower" == "paste" ]]; then
        paste_idx="$i"
        break
      fi
    done
    (( paste_idx >= 0 )) || bail "--paste requested but no [[paste]] section in $CONFIG_PATH."
    CONFIG_PATH="${multi_files[$paste_idx]}"
    main_config_label="${PROFILE_NAME}:${multi_names[$paste_idx]}"
  else
    CONFIG_PATH="${multi_files[0]}"
    main_config_label="${PROFILE_NAME}:${multi_names[0]}"
    if ((${#multi_files[@]} > 1)); then
      for i in $(seq 1 $((${#multi_files[@]} - 1))); do
        extra_configs+=("${multi_files[$i]}")
        extra_labels+=("${PROFILE_NAME}:${multi_names[$i]}")
      done
    fi
  fi
fi

INPUT_DIR="${INPUT_OVERRIDE:-$CAM_ROOT/input}"
OUTPUT_DIR="${OUTPUT_OVERRIDE:-$CAM_ROOT/output}"

mkdir -p "$OUTPUT_DIR"

if command -v "$PCB2GCODE_BIN" >/dev/null 2>&1; then
  PCB2GCODE_CMD="$(command -v "$PCB2GCODE_BIN")"
elif [[ -x "$PCB2GCODE_BIN" ]]; then
  PCB2GCODE_CMD="$PCB2GCODE_BIN"
else
  bail "pcb2gcode binary \"$PCB2GCODE_BIN\" not found. Set PCB2GCODE_BIN or install pcb2gcode."
fi

front_from_config=$(awk -F= '/^front=/{print $2; exit}' "$CONFIG_PATH")
outline_from_config=$(awk -F= '/^outline=/{print $2; exit}' "$CONFIG_PATH")
drill_from_config=$(awk -F= '/^drill=/{print $2; exit}' "$CONFIG_PATH")

project_front=""
project_outline=""
project_drill=""
if [[ -n "$PROJECT_OVERRIDE" ]]; then
  project_front="$INPUT_DIR/${PROJECT_OVERRIDE}-F_Cu.gbr"
  project_outline="$INPUT_DIR/${PROJECT_OVERRIDE}-Edge_Cuts.gbr"
  if [[ -n "$drill_from_config" ]]; then
    project_drill="$INPUT_DIR/${PROJECT_OVERRIDE}-PTH.drl"
  fi
fi

front_input="$(resolve_input_spec "${front_override:-$project_front}" "$INPUT_DIR")"
outline_input="$(resolve_input_spec "${outline_override:-$project_outline}" "$INPUT_DIR")"
drill_input="$(resolve_input_spec "${drill_override:-$project_drill}" "$INPUT_DIR")"

# Fall back to config-specified paths if no overrides or project hints were given.
[[ -z "$front_input" && -n "$front_from_config" ]] && front_input="$front_from_config"
[[ -z "$outline_input" && -n "$outline_from_config" ]] && outline_input="$outline_from_config"
[[ -z "$drill_input" && -n "$drill_from_config" ]] && drill_input="$drill_from_config"

if [[ -n "$drill_input" && "$drill_input" == *"-PTH.drl" ]]; then
  maybe_combined="${drill_input/-PTH.drl/.drl}"
  npth_file="${drill_input/-PTH.drl/-NPTH.drl}"
  if [[ -f "$maybe_combined" ]]; then
    echo "Using combined drill file: $maybe_combined" >&2
    drill_input="$maybe_combined"
  elif [[ -f "$npth_file" ]]; then
    # Create a persistent combined drill so NPTH mounting holes are included.
    combined_path="$maybe_combined"
    python3 - "$drill_input" "$npth_file" "$combined_path" <<'PY'
import re, sys
pth_path, npth_path, out_path = sys.argv[1:]

def read_strip_m30(path):
    lines = open(path, encoding="utf-8").read().splitlines()
    if lines and lines[-1].strip().upper() == "M30":
        lines = lines[:-1]
    return lines

pth_lines = read_strip_m30(pth_path)
npth_lines = read_strip_m30(npth_path)

tool_re = re.compile(r"^T(\d+)")
tool_def_re = re.compile(r"^T(\d+)C([0-9.]+)")

pth_tools = []
for line in pth_lines:
    m = tool_def_re.match(line.strip())
    if m:
        pth_tools.append(int(m.group(1)))

offset = max(pth_tools) if pth_tools else 0

def should_skip(line):
    return line.startswith(("M48", "FMAT", "METRIC", "%", ";"))

out = []
out.extend(pth_lines)

for line in npth_lines:
    if should_skip(line):
        continue
    mdef = tool_def_re.match(line.strip())
    if mdef:
        new_num = offset + int(mdef.group(1))
        out.append(f"T{new_num}C{mdef.group(2)}")
        continue
    mtool = tool_re.match(line.strip())
    if mtool:
        new_num = offset + int(mtool.group(1))
        rest = line.strip()[len(mtool.group(0)):]
        out.append(f"T{new_num}{rest}")
        continue
    out.append(line)

out.append("M30")
with open(out_path, "w", encoding="utf-8", newline="\n") as f:
    for l in out:
        f.write(l.rstrip() + "\n")
PY
    echo "Using combined drill file (PTH+NPTH): $combined_path" >&2
    drill_input="$combined_path"
  fi
fi

# If the requested drill file is missing, try obvious fallbacks (PTH/combined).
if [[ -n "$drill_input" && ! -f "$drill_input" ]]; then
  if [[ "$drill_input" == *.drl ]]; then
    base="${drill_input%.drl}"
    for alt in "${base}-PTH.drl" "${base}-NPTH.drl"; do
      if [[ -f "$alt" ]]; then
        echo "Using fallback drill file: $alt" >&2
        drill_input="$alt"
        break
      fi
    done
  fi
fi

if $paste_mode; then
  if [[ -n "${front_override:-}" || -n "${project_front:-}" ]]; then
    : # user already set a paste/front override.
  elif [[ -n "$front_from_config" ]]; then
    if [[ "$front_from_config" == *-F_Paste.gbr ]]; then
      front_input="$front_from_config"
    elif [[ "$front_from_config" == *-F_Cu.gbr ]]; then
      front_input="${front_from_config/-F_Cu.gbr/-F_Paste.gbr}"
    else
      bail "--paste expects the config front filename to end with -F_Cu.gbr or -F_Paste.gbr (got $front_from_config)."
    fi
  else
    bail "--paste requested but unable to infer base front filename."
  fi
  outline_input=""
  drill_input=""
fi

missing_inputs=()
[[ -n "$front_input" && ! -f "$front_input" ]] && missing_inputs+=("$front_input")
[[ -n "$outline_input" && ! -f "$outline_input" ]] && missing_inputs+=("$outline_input")
if [[ -n "$drill_input" ]]; then
  [[ -f "$drill_input" ]] || missing_inputs+=("$drill_input")
fi

if ((${#missing_inputs[@]} > 0)); then
  red='\e[31m'
  reset='\e[0m'
  printf "${red}ERROR: Expected Gerber/Drill files not found:${reset}\n" >&2
  for file in "${missing_inputs[@]}"; do
    printf "  %s\n" "$file" >&2
  done
  printf "Re-run KiCad Plot/Drill into %s or adjust --project/--front/--outline/--drill.\n" "$INPUT_DIR" >&2
  exit 1
fi

if ! $paste_mode && [[ ${#extra_configs[@]} -eq 0 ]]; then
  auto_paste_profile="$PROFILE_DIR/paste_stencil.millproject"
  auto_paste_front_path=""
  auto_paste_reason=""
  if [[ -f "$auto_paste_profile" ]]; then
    paste_front_from_config=$(awk -F= '/^front=/{print $2; exit}' "$auto_paste_profile")
    if [[ -n "$paste_front_from_config" ]]; then
      paste_front_resolved="$(resolve_relative "$paste_front_from_config")"
      if [[ -f "$paste_front_resolved" ]]; then
        extra_configs+=("$auto_paste_profile")
        extra_labels+=("auto_paste")
        auto_paste_front_path="$paste_front_resolved"
      else
        auto_paste_reason="missing front file ${paste_front_from_config}"
      fi
    else
      auto_paste_reason="paste profile missing front path"
    fi
  fi
fi

bold='\e[1m'
cyan='\e[36m'
green='\e[32m'
yellow='\e[33m'
red='\e[31m'
reset='\e[0m'

printf "${bold}${cyan}pcb2gcode workflow${reset}\n"
printf "${bold}Board:${reset} %s\n" "$BOARD_NAME"
printf "${bold}Profile:${reset} %s\n" "${CONFIG_OVERRIDE:+(custom)}${CONFIG_OVERRIDE:-$PROFILE_NAME}"
printf "${bold}Config:${reset} %s\n" "$config_display_path"
printf "${bold}Binary:${reset} %s\n" "$PCB2GCODE_CMD"
printf "${bold}Input dir:${reset} %s\n" "$INPUT_DIR"
printf "${bold}Output dir:${reset} %s\n" "$OUTPUT_DIR"
printf "${bold}Config sources:${reset} front=%s outline=%s drill=%s\n" \
  "${front_from_config:-n/a}" "${outline_from_config:-n/a}" "${drill_from_config:-n/a}"
if [[ -n "$PROJECT_OVERRIDE" || -n "$front_override" || -n "$outline_override" || -n "$drill_override" ]]; then
  printf "${bold}CLI overrides:${reset} front=%s outline=%s drill=%s\n" \
    "${front_input:-'-'}" "${outline_input:-'-'}" "${drill_input:-'-'}"
fi
if $paste_mode; then
  printf "${bold}Paste mode:${reset} front=%s (outline/drill skipped)\n" "${front_input:-'-'}"
elif ((${#extra_configs[@]} > 0)); then
  printf "${bold}Additional stages:${reset}\n"
  for idx in "${!extra_configs[@]}"; do
    extra_front=$(awk -F= '/^front=/{print $2; exit}' "${extra_configs[$idx]}")
    printf "  %s (front=%s)\n" "${extra_labels[$idx]}" "${extra_front:-n/a}"
  done
elif [[ -n "${auto_paste_reason:-}" ]]; then
  printf "${bold}Auto paste:${reset} skipped (%s)\n" "$auto_paste_reason"
fi

cmd=("$PCB2GCODE_CMD" --config "$CONFIG_PATH" --output-dir "$OUTPUT_DIR")
[[ -n "$front_input" ]] && cmd+=(--front "$front_input")
[[ -n "$outline_input" ]] && cmd+=(--outline "$outline_input")
[[ -n "$drill_input" ]] && cmd+=(--drill "$drill_input")
cmd+=("${passthrough_args[@]}")

run_pcb2gcode_stage "$main_config_label" "${cmd[@]}"

if ! $paste_mode && ((${#extra_configs[@]} > 0)); then
  for idx in "${!extra_configs[@]}"; do
    extra_cmd=("$PCB2GCODE_CMD" --config "${extra_configs[$idx]}" --output-dir "$OUTPUT_DIR")
    extra_cmd+=("${passthrough_args[@]}")
    run_pcb2gcode_stage "${extra_labels[$idx]}" "${extra_cmd[@]}"
  done
fi

shopt -s nullglob
for file in "$OUTPUT_DIR"/*.nc; do
  if grep -Eq '^[[:space:]]*M0([[:space:]]|$)' "$file"; then
    tmp="$(mktemp)"
    LC_ALL=C grep -Ev '^[[:space:]]*M0([[:space:]]|$)' "$file" >"$tmp"
    mv "$tmp" "$file"
    printf "${yellow}Stripped M0 pauses from %s${reset}\n" "$file"
  fi
done

printf "${green}Done. Review SVG previews in %s before machining.${reset}\n" "$OUTPUT_DIR"
