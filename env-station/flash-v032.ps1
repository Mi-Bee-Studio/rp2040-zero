# rp2040-zero v0.3.2 flash orchestrator:
# waits for the board (runtime CDC or BOOTSEL drive), flashes v0.3.2, resumes collector, reports telemetry.
$uf2 = 'C:\Users\micke\Projects\embedded\rp2040-zero\env-station\build\main\env-station.uf2'
$deadline = (Get-Date).AddMinutes(14)
$flashed = $false
while ((Get-Date) -lt $deadline -and -not $flashed) {
  $d = Get-CimInstance Win32_LogicalDisk -Filter "VolumeName='RPI-RP2'" | Select-Object -First 1
  if ($d) {
    Write-Output ('BOOTSEL drive at ' + $d.DeviceID + ' - copying UF2...')
    try { Copy-Item $uf2 -Destination ($d.DeviceID + '\'); Write-Output 'UF2 copied'; $flashed = $true }
    catch { Write-Output ('copy failed: ' + $_.Exception.Message) }
    Start-Sleep -Seconds 3
    continue
  }
  $cdc = Get-PnpDevice -InstanceId 'USB\VID_2E8A&PID_000A&MI_00*' -ErrorAction SilentlyContinue | Where-Object { $_.Present } | Select-Object -First 1
  if ($cdc) {
    # runtime present: pause collector, touch 1200 to enter BOOTSEL
    $com = ($cdc.FriendlyName -replace '.*\((COM\d+)\).*', '$1')
    if ($com -match '^COM\d+$') {
      Write-Output ('runtime on ' + $com + ' - pausing collector...')
      & 'C:\Users\micke\Projects\embedded\serialtap\serialtap.exe' pause '^rp2040-zero$' 2>&1 | Out-Null
      Start-Sleep -Seconds 2
      Write-Output 'touching 1200bps...'
      try {
        $p = New-Object System.IO.Ports.SerialPort($com, 1200)
        $p.Open(); Start-Sleep -Milliseconds 200; $p.Close()
        Write-Output 'touch sent - waiting for BOOTSEL drive...'
      } catch { Write-Output ('touch failed: ' + $_.Exception.Message) }
      # give the board ~8s to re-appear as a drive; then loop retries handle it
      Start-Sleep -Seconds 8
    }
    continue
  }
  Start-Sleep -Milliseconds 900
}
if (-not $flashed) { Write-Output 'TIMEOUT: board never returned in 14 min'; exit 1 }
Start-Sleep -Seconds 10
& 'C:\Users\micke\Projects\embedded\serialtap\serialtap.exe' resume '^rp2040-zero$' 2>&1 | Out-Null
Write-Output 'collector resumed - waiting for telemetry...'
Start-Sleep -Seconds 15
Get-Content 'C:\Users\micke\Projects\embedded\serialtap\logs\rp2040-zero\serial-20260927.log' -Tail 6 | ForEach-Object { Write-Output ('  ' + $_) }
