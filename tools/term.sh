#!/usr/bin/env bash
# Öffnet ein Terminal zur Platine (USB-CDC, z. B. für die Test-Shell).
# Findet die seriellen Ports der Platine selbst, bei mehreren fragt es nach.
#   term.sh               Port suchen und tio öffnen
#   term.sh /dev/ttyACM1  bestimmten Port öffnen
# Beenden mit Ctrl+T, dann Q.
set -euo pipefail

port="${1:-}"

if [[ -z "$port" ]]; then
    ports=(); labels=()
    for tty in /sys/class/tty/ttyACM*; do
        [[ -e "$tty/device" ]] || continue
        usb="$(readlink -f "$tty/device/..")"   # USB-Gerät über der Schnittstelle
        [[ "$(cat "$usb/idVendor" 2>/dev/null)" == 0483 ]] || continue
        name="/dev/$(basename "$tty")"
        ports+=("$name")
        labels+=("$name  $(cat "$usb/product" 2>/dev/null || echo '?')")
    done

    if [[ ${#ports[@]} -eq 0 ]]; then
        echo "Keine Platine gefunden. Läuft die Firmware, und ist das Gerät 0483:5740 an WSL durchgereicht?" >&2
        exit 1
    elif [[ ${#ports[@]} -eq 1 ]]; then
        port="${ports[0]}"
    else
        echo "Mehrere Ports gefunden:"
        PS3="Nummer: "
        select _ in "${labels[@]}"; do
            [[ -n "${REPLY//[^0-9]/}" && $REPLY -ge 1 && $REPLY -le ${#ports[@]} ]] && { port="${ports[$((REPLY - 1))]}"; break; }
        done
    fi
fi

SUDO=""
[[ -w "$port" ]] || SUDO="sudo"
echo "Verbinde mit $port (Enter drücken für die Eingabeaufforderung, Ende mit Ctrl+T Q)"
exec $SUDO tio "$port"
