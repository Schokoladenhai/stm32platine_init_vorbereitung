<#
.SYNOPSIS
  Reicht ST-Link, DFU-Bootloader und die Platine (USB-CDC) per usbipd an WSL durch.

.DESCRIPTION
  Zeigt alle angeschlossenen ST-Geräte (VID 0483) zur Auswahl, gibt sie bei Bedarf
  einmalig frei (bind, braucht Adminrechte) und hängt sie mit --auto-attach an WSL.
  Auto-attach hängt ein Gerät nach dem Abstecken oder einem Wechsel in den
  Bootloader automatisch wieder an, solange das Skript-Fenster läuft.

.EXAMPLE
  .\tools\usb-wsl.ps1          Geräte auswählen und durchreichen
  .\tools\usb-wsl.ps1 -All     alle ST-Geräte ohne Rückfrage
  .\tools\usb-wsl.ps1 -Stop    Durchreichen beenden und Geräte lösen

.NOTES
  Voraussetzung: usbipd-win ab Version 4 (winget install usbipd).
#>
param(
    [switch]$All,
    [switch]$Stop
)

$ErrorActionPreference = 'Stop'
$pidFile = Join-Path $env:TEMP 'usb-wsl.pids'

if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    Write-Host 'usbipd fehlt. Installieren mit:  winget install usbipd' -ForegroundColor Red
    exit 1
}

if ($Stop) {
    if (Test-Path $pidFile) {
        foreach ($line in Get-Content $pidFile) {
            $id, $busId = $line -split ' '
            Stop-Process -Id $id -ErrorAction SilentlyContinue
            usbipd detach --busid $busId 2>$null
        }
        Remove-Item $pidFile
    }
    Write-Host 'Durchreichen beendet.'
    exit 0
}

# Zustand als JSON lesen statt die Textausgabe von "usbipd list" zu zerlegen
$state = usbipd state | ConvertFrom-Json
$devices = @($state.Devices | Where-Object { $_.BusId -and $_.InstanceId -match 'VID_0483' })

if ($devices.Count -eq 0) {
    Write-Host 'Kein ST-Gerät angeschlossen (ST-Link, Bootloader oder Platine).' -ForegroundColor Yellow
    exit 1
}

for ($i = 0; $i -lt $devices.Count; $i++) {
    $d = $devices[$i]
    $vidpid = if ($d.InstanceId -match 'VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') { "$($Matches[1]):$($Matches[2])".ToLower() } else { '?' }
    $bound = if ($d.PersistedGuid) { 'freigegeben' } else { 'nicht freigegeben' }
    Write-Host ("[{0}] {1,-6} {2}  {3}  ({4})" -f ($i + 1), $d.BusId, $vidpid, $d.Description, $bound)
}

if ($All) {
    $selected = $devices
} else {
    $answer = Read-Host 'Nummern mit Komma (z. B. 1,2) oder a fuer alle'
    if ($answer -eq 'a') {
        $selected = $devices
    } else {
        $selected = @($answer -split ',' | ForEach-Object { $devices[[int]$_.Trim() - 1] })
    }
}

foreach ($d in $selected) {
    if (-not $d.PersistedGuid) {
        Write-Host "Gebe $($d.BusId) frei (Adminrechte noetig) ..."
        Start-Process usbipd -ArgumentList "bind --busid $($d.BusId)" -Verb RunAs -Wait
    }
    $p = Start-Process usbipd -ArgumentList "attach --wsl --busid $($d.BusId) --auto-attach" -WindowStyle Minimized -PassThru
    Add-Content $pidFile "$($p.Id) $($d.BusId)"
    Write-Host "$($d.BusId) haengt jetzt an WSL (auto-attach laeuft im Hintergrund)."
}

Write-Host 'Pruefen in WSL bzw. im Container mit: lsusb'
