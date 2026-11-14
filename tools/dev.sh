#!/usr/bin/env bash
# Startet die Entwicklungsumgebung ohne IDE und ohne devcontainer-CLI.
# Baut das Image aus .devcontainer/Dockerfile (nur beim ersten Mal langsam)
# und öffnet eine Shell im Container mit dem Repo unter /workspace.
#   dev.sh            interaktive Shell
#   dev.sh build.sh   einzelnen Befehl im Container ausführen
# In WSL bzw. Linux aufrufen, nicht aus Windows heraus.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE=stm32platine-dev

docker build -q -t "$IMAGE" "$ROOT/.devcontainer" >/dev/null

tty_flags=(-i)
[[ -t 0 ]] && tty_flags=(-it)

exec docker run --rm "${tty_flags[@]}" \
    -v "$ROOT":/workspace \
    -v /dev:/dev \
    --device-cgroup-rule='c 189:* rmw' \
    --device-cgroup-rule='c 166:* rmw' \
    "$IMAGE" "${@:-bash}"
