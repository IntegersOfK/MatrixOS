# Phase 0: read-only interrogation + full flash backup of the Mystrix.
# Board must already be in ROM download mode (FLASH header pin 2 BOOT bridged to pin 1 GND at plug-in).
# Nothing here writes flash or eFuses: chip_id, flash_id, read_flash, verify_flash, espefuse summary only.
$ErrorActionPreference = 'Stop'
$py = 'C:\Espressif\python_env\idf5.3_py3.12_env\Scripts\python.exe'
$repo = 'C:\Users\ajwes\repos\MatrixOS'
$stamp = Get-Date -Format 'yyyyMMdd'
$out = Join-Path $repo "Custom\backups\$stamp"
New-Item -ItemType Directory -Force $out | Out-Null

# ROM download mode enumerates as Espressif USB-Serial-JTAG (VID 303A)
$port = Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -match 'VID_303A' -and $_.Name -match '\((COM\d+)\)' } |
  ForEach-Object { $Matches[1] } | Select-Object -First 1
if (-not $port) { throw 'No VID_303A serial port found; board is not in ROM download mode.' }
"Port: $port" | Tee-Object "$out\log.txt"

$common = @('--chip', 'esp32s3', '-p', $port, '--before', 'no_reset', '--after', 'no_reset')
& $py -m esptool @common chip_id 2>&1 | Tee-Object -Append "$out\log.txt"
& $py -m esptool @common flash_id 2>&1 | Tee-Object -Append "$out\log.txt"
& $py -m espefuse --chip esp32s3 -p $port --before no_reset summary 2>&1 | Tee-Object "$out\efuse-summary.txt"
& $py -m espefuse --chip esp32s3 -p $port --before no_reset dump 2>&1 | Tee-Object "$out\efuse-dump.txt"

$img = "$out\mystrix-$stamp.bin"
& $py -m esptool @common read_flash 0 0x800000 $img 2>&1 | Tee-Object -Append "$out\log.txt"
# Second, independent check: compare the whole chip against the dump (read-only)
& $py -m esptool @common verify_flash 0 $img 2>&1 | Tee-Object -Append "$out\log.txt"
(Get-FileHash $img -Algorithm SHA256).Hash | Tee-Object "$img.sha256"
