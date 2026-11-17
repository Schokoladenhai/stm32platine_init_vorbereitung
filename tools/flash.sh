#!/usr/bin/env bash
# Flasht die Firmware per ST-Link (SWD) oder USB-DFU und lässt dich das Gerät auswählen.
#   flash.sh                   Debug-Build, Gerät automatisch bzw. per Auswahl
#   flash.sh --test            Test-Build (lars_fw_test)
#   flash.sh --release         Release-Build
#   flash.sh swd | dfu         nur Geräte dieser Art anbieten
#   flash.sh --device <SN>     Gerät per Seriennummer, ohne Rückfrage
#   flash.sh --yes             nie nachfragen (CI, NUC); bei mehreren Geräten Abbruch
# Vorher bauen mit build.sh (gleiche Optionen).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
preset=debug
name=lars_fw
mode=any
want_serial=""
assume_yes=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --test) preset=test; name=lars_fw_test ;;
        --release) preset=release ;;
        swd|dfu) mode="$1" ;;
        --device) want_serial="${2:?Seriennummer fehlt}"; shift ;;
        --yes|-y) assume_yes=1 ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unbekannte Option: $1" >&2; exit 1 ;;
    esac
    shift
done

elf="$ROOT/build/$preset/$name.elf"
bin="$ROOT/build/$preset/$name.bin"
if [[ ! -f "$elf" ]]; then
    echo "$elf fehlt. Zuerst bauen: build.sh $([[ $preset != debug ]] && echo "--$preset")" >&2
    exit 1
fi

# Bekannte Geräte (VID:PID -> Art)
declare -A KNOWN=(
    [0483:3748]="swd|ST-Link V2"
    [0483:374b]="swd|ST-Link V2-1"
    [0483:3752]="swd|ST-Link V2-1"
    [0483:374e]="swd|ST-Link V3"
    [0483:374f]="swd|ST-Link V3"
    [0483:3753]="swd|ST-Link V3"
    [0483:df11]="dfu|STM32 Bootloader (DFU)"
)

# Geräte über sysfs suchen (funktioniert ohne udev-Regeln, auch im Container)
kinds=(); labels=(); serials=(); locations=()
for dev in "${USB_SYSFS:-/sys/bus/usb/devices}"/*; do
    [[ -f "$dev/idVendor" ]] || continue
    id="$(cat "$dev/idVendor"):$(cat "$dev/idProduct")"
    [[ -n "${KNOWN[$id]:-}" ]] || continue
    kind="${KNOWN[$id]%%|*}"
    [[ $mode == any || $mode == "$kind" ]] || continue
    serial="$(cat "$dev/serial" 2>/dev/null || true)"
    # Ältere ST-Link V2 liefern die Seriennummer als Binärdaten. Die taugt nicht zur
    # Auswahl, dann wird das Gerät über seinen USB-Anschluss (z. B. 2-1.3) angesprochen.
    [[ "$serial" =~ ^[0-9A-Za-z]+$ ]] || serial=""
    [[ -z "$want_serial" || "$serial" == "$want_serial" ]] || continue
    location="$(basename "$dev")"
    kinds+=("$kind")
    labels+=("${KNOWN[$id]#*|}  (USB $location, SN ${serial:-nicht lesbar})")
    serials+=("$serial")
    locations+=("$location")
done

if [[ ${#kinds[@]} -eq 0 ]]; then
    echo "Kein passendes Gerät gefunden." >&2
    echo "Unter Windows: Gerät mit tools/usb-wsl.ps1 an WSL durchreichen. DFU: BOOT0 auf 3V3 und Reset." >&2
    exit 1
fi

choice=0
if [[ ${#kinds[@]} -gt 1 ]]; then
    if [[ $assume_yes -eq 1 ]]; then
        echo "Mehrere Geräte gefunden, mit --device <SN> eines wählen:" >&2
        printf '  %s\n' "${labels[@]}" >&2
        exit 1
    fi
    echo "Mehrere Geräte gefunden:"
    PS3="Nummer: "
    select _ in "${labels[@]}"; do
        [[ -n "${REPLY//[^0-9]/}" && $REPLY -ge 1 && $REPLY -le ${#kinds[@]} ]] && { choice=$((REPLY - 1)); break; }
    done
fi

kind="${kinds[$choice]}"
serial="${serials[$choice]}"
echo "Flashe $name per ${kind^^} über: ${labels[$choice]}"

SUDO=""
[[ $(id -u) -ne 0 ]] && SUDO="sudo"

if [[ $kind == swd ]]; then
    if [[ -n "$serial" ]]; then
        select_cmd=(-c "adapter serial $serial")
    else
        select_cmd=(-c "adapter usb location ${locations[$choice]}")
    fi
    $SUDO openocd -f interface/stlink.cfg "${select_cmd[@]}" -f target/stm32f4x.cfg \
        -c "program $elf verify reset exit"
else
    serial_arg=(-p "${locations[$choice]}")
    [[ -n "$serial" ]] && serial_arg=(-S "$serial")
    $SUDO dfu-util -d 0483:df11 "${serial_arg[@]}" -a 0 -s 0x08000000:leave -D "$bin"
    echo "Fertig. BOOT0-Jumper entfernen und Platine einmal stromlos machen."
fi
