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
| `tools/` | `dev.sh`, `build.sh`, `flash.sh`, `usb-wsl.cmd`/`.ps1` |
| `.devcontainer/` | Entwicklungsumgebung |

Später kommen `drivers/`, `bus/` und `table/` dazu.

## Entwicklungsumgebung

Voraussetzung ist Docker, unter Windows in WSL 2. Das Repo wird **in WSL** geklont
(z. B. `~/stm32platine`) und auch aus WSL heraus geöffnet, nicht über ein Windows-Laufwerk.
Docker kann einen Ordner, der über `W:\` oder `\\wsl.localhost\…` geöffnet wurde, nicht einbinden.

- **Ohne IDE (immer möglich):** in WSL `tools/dev.sh` ausführen. Das baut das Image und öffnet
  eine Shell im Container. Jeder Editor kann die Dateien dabei normal bearbeiten.
- **Mit IDE:** Ordner über die WSL-Anbindung der IDE öffnen und „In Container öffnen“ wählen
  (`.devcontainer/devcontainer.json`, ohne IDE-spezifische Einstellungen).

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
.\tools\usb-wsl.cmd          # Geräte auswählen (oder Doppelklick im Explorer)
.\tools\usb-wsl.cmd -Stop    # Durchreichen beenden
```

`usb-wsl.cmd` startet `usb-wsl.ps1` mit `-ExecutionPolicy Bypass`, weil Windows Skripte
standardmäßig blockiert. Die Richtlinie des Systems bleibt dabei unverändert.

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
**grün** (Debug-Build) bzw. **blau** (Test-Build). Im Release-Build blinkt nichts.

## CubeMX

STM32CubeMX 6.x (nicht CubeMX2), FW_F4 V1.28.3. CubeMX läuft unter Windows und kann
keine WSL-Pfade auswählen. Deshalb WSL einmalig als Laufwerk einbinden:

```
net use W: \\wsl.localhost\Ubuntu /persistent:yes
```

Dann `W:\home\<user>\<repo>\cubemx\HardwareInterface.ioc` öffnen, ändern und „Generate Code“.
Eigener Code gehört nie nach `cubemx/`. Die Start-Task heißt `app` mit `app_main` als „As weak“.
Die echte Funktion steht in `app/app.cpp`.
