#!/usr/bin/env bash
# Öffnet ein Terminal zur Platine (USB-CDC, z. B. für die Test-Shell).
# Findet die seriellen Ports der Platine selbst, bei mehreren fragt es nach.
#   term.sh               Port suchen und verbinden
#   term.sh /dev/ttyACM1  bestimmten Port öffnen
# Beenden mit „exit“ oder Ctrl+C.
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

# Ohne Rechte auf den Port das ganze Skript mit sudo neu starten
if [[ ! -r "$port" || ! -w "$port" ]]; then
    exec sudo "$(readlink -f "$0")" "$port"
fi

echo "Verbunden mit $port. Enter für die Eingabeaufforderung, Ende mit exit oder Ctrl+C."

stty -F "$port" raw -echo
exec 3<>"$port"
cat <&3 &                           # Ausgabe der Platine (inklusive Echo der Eingabe)
reader=$!

saved="$(stty -g)"
cleanup() { stty "$saved"; kill "$reader" 2>/dev/null || true; echo; }
trap cleanup EXIT
stty raw -echo                      # jede Taste sofort weitergeben, die Platine schickt das Echo

line=""
while IFS= read -rn1 c; do
    case "$c" in
        $'\x03') break ;;                               # Ctrl+C
        $'\r' | "")                                     # Enter
            printf '\r' >&3
            if [[ "$line" == exit ]]; then
                sleep 0.2                               # „OK“ der Platine noch anzeigen
                break
            fi
            line="" ;;
        $'\x7f' | $'\b') line="${line%?}"; printf '%s' "$c" >&3 ;;
        *) line+="$c"; printf '%s' "$c" >&3 ;;
    esac
done
