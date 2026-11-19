# GUI-Rauchtest der installierten/entpackten schneefall.exe (CI-Installer-Test und Windows-VM):
# Programm mit GUI starten (kein QT_QPA_PLATFORM=offscreen: qwindows-Plugin und ggf. opengl32sw sollen wirklich geladen werden),
# ein paar Sekunden warten, prüfen: Prozess läuft, qwindows.dll ist geladen, --status antwortet; dann --beenden --still.
# Aufruf: pwsh -File gui-test.ps1 -Exe <Pfad\schneefall.exe> [-Warten 6]
param([Parameter(Mandatory)][string]$Exe, [int]$Warten = 6)
$ErrorActionPreference = 'Stop'
$log = Join-Path ([IO.Path]::GetTempPath()) "schneefall-gui-$PID"
$env:QT_DEBUG_PLUGINS = '1'
$p = Start-Process $Exe -PassThru -RedirectStandardError "$log.err.txt" -RedirectStandardOutput "$log.out.txt"
$env:QT_DEBUG_PLUGINS = $null
function Fertig([string]$grund) {
    Write-Host "== Plugin-Log (Auszug)"; if (Test-Path "$log.err.txt") { Get-Content "$log.err.txt" -TotalCount 60 }
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    throw $grund
}
Start-Sleep -Seconds $Warten
if ($p.HasExited) { $p.WaitForExit(); Fertig "schneefall.exe endete nach dem Start (Exit $($p.ExitCode))" }
$module = (Get-Process -Id $p.Id).Modules | ForEach-Object { $_.ModuleName.ToLower() }
$log_text = if (Test-Path "$log.err.txt") { Get-Content "$log.err.txt" -Raw } else { '' }
if ($module -notcontains 'qwindows.dll' -and $log_text -notmatch 'qwindows') { Fertig 'qwindows.dll wurde nicht geladen' }
Write-Host ("geladen: qwindows.dll = {0}, opengl32sw.dll = {1}, Qt6OpenGL.dll = {2}" -f ($module -contains 'qwindows.dll'), ($module -contains 'opengl32sw.dll'), ($module -contains 'qt6opengl.dll'))
$s = Start-Process $Exe -ArgumentList '--status' -Wait -PassThru -RedirectStandardOutput "$log.status.txt"
if ($s.ExitCode -ne 0) { Fertig "--status antwortet nicht (Exit $($s.ExitCode))" }
Write-Host ("--status: " + ((Get-Content "$log.status.txt" -Raw) -replace '\s+', ' ').Trim())
Start-Process $Exe -ArgumentList '--beenden', '--still' -Wait | Out-Null
if (-not $p.WaitForExit(15000)) { Fertig 'Programm hat sich nach --beenden nicht beendet' }
Remove-Item "$log.*" -ErrorAction SilentlyContinue
Write-Host 'GUI-Test bestanden'
