<#
.SYNOPSIS
    Run Verium Miner headless at startup via a Windows Scheduled Task.

.DESCRIPTION
    cpuminer.exe is a console application, so the reliable built-in way to run
    it unattended at boot is a Scheduled Task (a console EXE registered as a
    true SCM service typically fails with error 1053). This script creates a
    task that launches the miner minimized at system startup and restarts it if
    it ever exits.

    The miner reads its configuration from
    %APPDATA%\cpuminer\cpuminer-conf.json (run `cpuminer.exe --setup` first, or
    copy and edit the bundled cpuminer-conf.json), unless you pass -Config.

    Run from an elevated (Administrator) PowerShell prompt.

.EXAMPLE
    .\install-service.ps1
    .\install-service.ps1 -ExePath "C:\veriumminer\cpuminer.exe" -Config "C:\veriumminer\cpuminer-conf.json"

.NOTES
    Uninstall:  .\install-service.ps1 -Uninstall
    Prefer a true service? Use NSSM (https://nssm.cc): nssm install VeriumMiner "C:\veriumminer\cpuminer.exe"
#>
[CmdletBinding()]
param(
    [string]$ExePath = (Join-Path $PSScriptRoot 'cpuminer.exe'),
    [string]$Config,
    [string]$TaskName = 'VeriumMiner',
    [switch]$Uninstall
)

$ErrorActionPreference = 'Stop'

function Assert-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p  = New-Object Security.Principal.WindowsPrincipal($id)
    if (-not $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw 'Please run this script from an elevated (Administrator) PowerShell.'
    }
}

Assert-Admin

if ($Uninstall) {
    if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
        Write-Host "Removed scheduled task '$TaskName'."
    } else {
        Write-Host "Scheduled task '$TaskName' is not installed."
    }
    return
}

if (-not (Test-Path $ExePath)) {
    throw "cpuminer.exe not found at '$ExePath'. Use -ExePath to point at it."
}

$argList = '-B'
if ($Config) {
    if (-not (Test-Path $Config)) { throw "Config not found: $Config" }
    $argList = "-B -c `"$Config`""
}

$action   = New-ScheduledTaskAction -Execute $ExePath -Argument $argList `
                -WorkingDirectory (Split-Path $ExePath)
$trigger  = New-ScheduledTaskTrigger -AtStartup
$principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount `
                -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries `
                -DontStopIfGoingOnBatteries -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) `
                -ExecutionTimeLimit (New-TimeSpan -Seconds 0)

if (Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
}

Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
    -Principal $principal -Settings $settings `
    -Description 'Verium CPU miner (scrypt-squared), headless.' | Out-Null

Start-ScheduledTask -TaskName $TaskName
Write-Host "Installed and started scheduled task '$TaskName'."
Write-Host "Manage with: Get-ScheduledTask $TaskName ; Stop-ScheduledTask $TaskName ; Start-ScheduledTask $TaskName"
