#!/usr/bin/env bash
# Öffnet ein Terminal zur Platine (USB-CDC, z. B. für die Test-Shell).
# Findet die seriellen Ports der Platine selbst, bei mehreren fragt es nach.
#   term.sh               Port suchen und tio öffnen
#   term.sh /dev/ttyACM1  bestimmten Port öffnen
# Beenden mit Ctrl+A, dann Q (andere Taste: TERM_KEY=x term.sh).
# Ctrl+T, die Vorgabe von tio, fangen manche Terminals selbst ab.
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
        echo "Gefunden: ${labels[0]}"
    else
        echo "Mehrere Ports gefunden:"
        PS3="Nummer: "
        select _ in "${labels[@]}"; do
            [[ -n "${REPLY//[^0-9]/}" && $REPLY -ge 1 && $REPLY -le ${#ports[@]} ]] && { port="${ports[$((REPLY - 1))]}"; break; }
        done
    fi
fi

SUDO=()
[[ -w "$port" ]] || SUDO=(sudo --preserve-env=XDG_CONFIG_HOME)
# tio 2.7 kennt die Taste nur aus einer Konfigdatei, deshalb eine temporäre.
key="${TERM_KEY:-a}"
cfg="$(mktemp -d)"
mkdir -p "$cfg/tio"
echo "prefix-ctrl-key = $key" > "$cfg/tio/config"
echo "Verbinde mit $port (Enter drücken für die Eingabeaufforderung, Ende mit Ctrl+${key^^}, dann Q)"
XDG_CONFIG_HOME="$cfg" "${SUDO[@]}" tio "$port" || true
rm -rf "$cfg"
