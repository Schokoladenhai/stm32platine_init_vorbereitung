#!/usr/bin/env bash
# Baut die Firmware über die CMake-Presets.
#   build.sh            Debug-Build    -> build/debug/lars_fw.elf
#   build.sh --release  Release-Build  -> build/release/lars_fw.elf
#   build.sh --test     Test-Build     -> build/test/lars_fw_test.elf
#   build.sh --clean    Build-Ordner des Presets vorher löschen
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
preset=debug
clean=0

for arg in "$@"; do
    case "$arg" in
        --test) preset=test ;;
        --release) preset=release ;;
        --clean) clean=1 ;;
        -h|--help) sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unbekannte Option: $arg" >&2; exit 1 ;;
    esac
done

cd "$ROOT"
if [[ $clean -eq 1 ]]; then
    rm -rf "build/$preset"
fi
cmake --preset "$preset" >/dev/null
cmake --build --preset "$preset"
