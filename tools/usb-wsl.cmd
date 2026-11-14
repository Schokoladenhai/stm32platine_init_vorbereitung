@echo off
rem Startet usb-wsl.ps1 ohne die Ausfuehrungsrichtlinie von Windows dauerhaft zu aendern.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0usb-wsl.ps1" %*
