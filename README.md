# Firmware STM32-Platine

Firmware für die Hardware-Interface-Platine (STM32F429ZI, FreeRTOS, C++17).
Aufbau und Begründungen stehen im Architekturkonzept der Studienarbeit.

> Vorbereitungs-Repo: Alles hier wird der erste Commit des eigentlichen Firmware-Repos.

## Ordner

| Ordner | Inhalt |
|---|---|
| `cubemx/` | Von STM32CubeMX generiert. Nur über die `.ioc` in CubeMX ändern. |
| `app/` | `app_main()` als Einstieg, Konfiguration, später die Tasks |
| `test/` | Nur im Test-Build: Kommando-Shell und Testdatei pro Treiber |
| `cmake/` | Toolchain-Datei für `arm-none-eabi-gcc` |
| `tools/` | `build.sh`, `flash.sh`, `usb-wsl.ps1` |
| `.devcontainer/` | Entwicklungsumgebung |

Später kommen `drivers/`, `bus/` und `table/` dazu.

## Entwicklungsumgebung

Voraussetzung ist Docker, unter Windows in WSL 2. Das Repo wird in WSL geklont.
Der Dev-Container ist an keine IDE gebunden.

- **Zed, VS Code:** Ordner öffnen und „In Container öffnen“ wählen.
- **Ohne IDE:** `devcontainer up --workspace-folder .` und `devcontainer exec --workspace-folder . bash`
  (Dev-Container-CLI, `npm install -g @devcontainers/cli`).

## Bauen

```bash
build.sh              # build/debug/lars_fw.elf
build.sh --test       # build/test/lars_fw_test.elf
build.sh --release    # build/release/lars_fw.elf
build.sh --clean      # vorher Build-Ordner löschen
```

Ohne Skript: `cmake --preset debug && cmake --build --preset debug`.

## USB-Geräte unter Windows an WSL durchreichen

Einmalig usbipd installieren: `winget install usbipd`. Danach in PowerShell im Repo-Ordner:

```powershell
.\tools\usb-wsl.ps1          # Geräte auswählen
.\tools\usb-wsl.ps1 -Stop    # Durchreichen beenden
```

Unter Linux ist nichts zu tun.

## Flashen

```bash
flash.sh              # Debug-Build
flash.sh --test       # Test-Build
flash.sh swd          # nur ST-Link anbieten
flash.sh dfu          # nur DFU-Bootloader anbieten
```

`flash.sh` sucht ST-Link und DFU-Bootloader selbst. Bei mehreren Geräten fragt es nach.
Für Skripte gibt es `--device <Seriennummer>` und `--yes`.

- **SWD (ST-Link):** BOOT0 offen. Geht immer, auch zum Debuggen.
- **DFU (USB):** BOOT0-Jumper auf 3V3, Reset, Kabel am Port „USB Host“. Nach dem Flashen Jumper ab.

Nach dem Flashen die Platine einmal stromlos machen. Läuft die Firmware, blinkt die Debug-LED
**grün** (normaler Build) bzw. **blau** (Test-Build).

## CubeMX

STM32CubeMX 6.x (nicht CubeMX2), FW_F4 V1.28.3. CubeMX läuft unter Windows und kann
keine WSL-Pfade auswählen. Deshalb WSL einmalig als Laufwerk einbinden:

```
net use W: \\wsl.localhost\Ubuntu /persistent:yes
```

Dann `W:\home\<user>\<repo>\cubemx\HardwareInterface.ioc` öffnen, ändern und „Generate Code“.
Eigener Code gehört nie nach `cubemx/`. Die Start-Task heißt `app` mit `app_main` als „As weak“.
Die echte Funktion steht in `app/app.cpp`.
