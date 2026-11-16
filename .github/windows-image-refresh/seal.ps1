# Seals the Windows image refresh maintenance VM: removes the maintenance runner and secrets, then
# generalizes Windows with Sysprep and shuts it down. Run as SYSTEM by a one-shot scheduled task that
# the warm-cache job registers, so the shutdown never strands a running GitHub job.
param(
    [Parameter(Mandatory = $true)][string]$UnattendPath,
    [Parameter(Mandatory = $true)][string]$InfoJson
)

$ErrorActionPreference = "Stop"

$StateDir = "C:\ImageRefresh"
$LogFile = Join-Path $StateDir "seal.log"
$RunnerDir = "C:\actions-runner"
$InfoDir = "C:\ProgramData\kDriveImageRefresh"

function Write-Log([string]$Message) {
    $line = "$(Get-Date -Format o) $Message"
    Add-Content -Path $LogFile -Value $line
    Write-Host $line
}

function Wait-RunnerIdle {
    $deadline = (Get-Date).AddMinutes(30)
    $idleSince = $null
    while ((Get-Date) -lt $deadline) {
        if (Get-Process -Name "Runner.Worker" -ErrorAction SilentlyContinue) {
            $idleSince = $null
        }
        elseif (-not $idleSince) {
            $idleSince = Get-Date
        }
        elseif (((Get-Date) - $idleSince).TotalSeconds -ge 60) {
            return
        }
        Start-Sleep -Seconds 10
    }
    throw "Runner did not become idle before the sealing deadline."
}

try {
    # Wait for the warm-cache job to be fully reported by the runner.
    Wait-RunnerIdle
    Write-Log "Runner idle; sealing."

    Unregister-ScheduledTask -TaskName "ActionsRunnerStart" -Confirm:$false -ErrorAction SilentlyContinue
    Unregister-ScheduledTask -TaskName "ImageRefreshResume" -Confirm:$false -ErrorAction SilentlyContinue
    Get-Process -Name "Runner.Listener", "Runner.Worker" -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Seconds 5

    # The GitHub registration itself is deleted by the workflow; only remove local runner state.
    foreach ($item in @(".runner", ".credentials", ".credentials_rsaparams", ".registered", ".env", ".path", "_work", "_diag")) {
        $path = Join-Path $RunnerDir $item
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }

    $winlogonKey = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon"
    Set-ItemProperty -Path $winlogonKey -Name AutoAdminLogon -Value "0"
    if (Get-ItemProperty -Path $winlogonKey -Name DefaultPassword -ErrorAction SilentlyContinue) {
        Remove-ItemProperty -Path $winlogonKey -Name DefaultPassword
    }

    Stop-Service -Name wuauserv -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "C:\Windows\SoftwareDistribution\Download\*" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -Path "$env:SystemRoot\Temp\*" -Recurse -Force -ErrorAction SilentlyContinue

    # Non-secret provenance, checked by the validation job and used to name this image when it is replaced.
    New-Item -Path $InfoDir -ItemType Directory -Force | Out-Null
    [System.Text.Encoding]::UTF8.GetString([System.Convert]::FromBase64String($InfoJson)) |
        Set-Content -Path (Join-Path $InfoDir "refresh-info.json") -Encoding UTF8
    Copy-Item -Path (Join-Path $StateDir "refresh.log") -Destination (Join-Path $InfoDir "windows-update.log") -Force

    Write-Log "Starting Sysprep with $UnattendPath"
    Copy-Item -Path $LogFile -Destination (Join-Path $InfoDir "seal.log") -Force
    Unregister-ScheduledTask -TaskName "ImageRefreshSeal" -Confirm:$false -ErrorAction SilentlyContinue
    Remove-Item -Path $StateDir -Recurse -Force

    & "$env:SystemRoot\System32\Sysprep\sysprep.exe" /generalize /oobe /shutdown /quiet "/unattend:$UnattendPath"
    if ($LASTEXITCODE -ne 0) { throw "Sysprep failed with exit code $LASTEXITCODE." }
}
catch {
    New-Item -Path $InfoDir -ItemType Directory -Force | Out-Null
    Add-Content -Path (Join-Path $InfoDir "seal-error.log") -Value ($_ | Out-String)
    throw
}
