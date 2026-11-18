# Firmware STM32-Platine

Firmware für die Hardware-Interface-Platine (STM32F429ZI, FreeRTOS, C++17).
Aufbau und Begründungen stehen im Architekturkonzept der Studienarbeit.

> Vorbereitungs-Repo: Alles hier wird der erste Commit des eigentlichen Firmware-Repos.

## Ordner

| Ordner | Inhalt |
|---|---|
| `cubemx/` | Von STM32CubeMX generiert. Nur über die `.ioc` in CubeMX ändern. |
| `app/` | `app_main()` als Einstieg, Konfiguration, später die Tasks |
| `bus/` | Threadsafe Zugriff auf geteilte Peripherie, z. B. `can_bus` (CAN1: senden aus jeder Task, Empfang per Filter in die Queue der Task) |
| `drivers/` | Treiber als Werkzeuge ohne eigene Task, z. B. `vesc` (Motor und Lenkung über den Servo-Ausgang des VESC) |
| `common/` | Gemeinsame Bausteine, z. B. `usb_cdc` (serielle Schnittstelle über USB) |
| `test/` | Nur im Test-Build: Kommando-Shell und Testdatei pro Treiber |
| `cmake/` | Toolchain-Datei für `arm-none-eabi-gcc` |
| `tools/` | `dev.sh`, `build.sh`, `flash.sh`, `term.sh`, `usb-wsl.cmd`/`.ps1` |
| `.devcontainer/` | Entwicklungsumgebung |

Später kommt `table/` dazu.

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

## Test-Shell

Der Test-Build startet eine Kommando-Shell über USB-CDC am Stecker „USB Host“, also am selben Stecker wie beim Flashen per DFU (USB FS, PA11/PA12). Der Stecker „USB Debug“ (USB HS) ist ungenutzt.

```bash
build.sh --test && flash.sh --test
term.sh               # findet den Port selbst, Ende mit exit oder Ctrl+C
```

Unter Windows muss dafür auch das Gerät `0483:5740` (laufende Firmware) mit `usb-wsl.cmd` durchgereicht sein.
Nach dem Verbinden einmal Enter drücken.

```
> help
> version
> tasks
> led red on
> can listen 0 0 x       # alle 29-Bit-IDs mithören (z. B. VESC-Status)
> can rx                 # empfangene Frames ausgeben
> can send 123 01 02     # Standard-Frame senden, ID und Bytes hex
> can stat               # Zähler, Fehlerzustand
> vesc scan              # welche VESC-IDs senden Status?
> vesc id 86             # VESC-ID für die folgenden Befehle
> vesc erpm 2000 3000    # 3 s lang 2000 ERPM (alle 20 ms wiederholt), danach Motor frei
> vesc servo 1520 2000   # Lenkservo 2 s auf 1520 µs (Werte <= 2 gelten als Position 0..1)
> vesc status            # Drehzahl, Ströme, Spannung, Temperaturen
```

Jede Antwort endet mit einer Zeile `OK …` oder `ERR …`, davor dürfen Datenzeilen stehen.
Neue Testbefehle für einen Treiber: `test/test_<treiber>.cpp` nach dem Muster von `test/test_led.cpp`
anlegen, in `CMakeLists.txt` und in der Tabelle in `test/test_main.cpp` eintragen.

## CubeMX

STM32CubeMX 6.x (nicht CubeMX2), FW_F4 V1.28.3. CubeMX läuft unter Windows und kann
keine WSL-Pfade auswählen. Deshalb WSL einmalig als Laufwerk einbinden:

```
net use W: \\wsl.localhost\Ubuntu /persistent:yes
```

Dann `W:\home\<user>\<repo>\cubemx\HardwareInterface.ioc` öffnen, ändern und „Generate Code“.
Eigener Code gehört nie nach `cubemx/`. Einzige Ausnahmen, beide im User-Code und damit sicher beim Generieren:
- `USB_DEVICE/App/usbd_cdc_if.c`: zwei Zeilen, die empfangene USB-Daten an `common/usb_cdc` weitergeben.
- `USB_DEVICE/App/usbd_desc.c`: setzt die USB-Seriennummer aus der Chip-ID, weil CubeMX das in `Get_SerialNum()` vergisst.

Die Start-Task heißt `app` mit `app_main` als „As weak“.
Die echte Funktion steht in `app/app.cpp`.
