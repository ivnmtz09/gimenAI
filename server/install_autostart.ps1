# Registra el lanzador de GimenAI para que arranque al iniciar sesión en Windows.
# Uso:  powershell -ExecutionPolicy Bypass -File install_autostart.ps1
# Quitar: Unregister-ScheduledTask -TaskName "GimenAI Launcher" -Confirm:$false
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$py = (Get-Command pythonw.exe -ErrorAction SilentlyContinue).Source
if (-not $py) { $py = (Get-Command python.exe).Source }

$action  = New-ScheduledTaskAction -Execute $py -Argument "`"$here\launcher.py`"" -WorkingDirectory $here
$trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
    -StartWhenAvailable -ExecutionTimeLimit ([TimeSpan]::Zero) -RestartCount 5 -RestartInterval (New-TimeSpan -Minutes 1)

Register-ScheduledTask -TaskName "GimenAI Launcher" -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
Start-ScheduledTask -TaskName "GimenAI Launcher"
Write-Host "Listo: 'GimenAI Launcher' registrado y en ejecución ($py)."
