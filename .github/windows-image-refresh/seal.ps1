# Seals the Windows image refresh maintenance VM: removes the maintenance runner and secrets, restarts until
# no servicing operation is pending, then generalizes Windows with Sysprep and shuts it down. Run as SYSTEM
# by a scheduled task that the warm-cache job registers, so the shutdown never strands a running GitHub job.
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

$SealStateFile = Join-Path $StateDir "seal-state.json"
$MaxSealRestarts = 3
$ServicingProcesses = @("TiWorker", "TrustedInstaller", "MoUsoCoreWorker", "wuauclt", "mscorsvw", "ngen", "ngentask")

function Wait-ServicingIdle {
    $deadline = (Get-Date).AddMinutes(60)
    $idleSince = $null
    while ((Get-Date) -lt $deadline) {
        if (Get-Process -Name $ServicingProcesses -ErrorAction SilentlyContinue) {
            $idleSince = $null
        }
        elseif (-not $idleSince) {
            $idleSince = Get-Date
        }
        elseif (((Get-Date) - $idleSince).TotalSeconds -ge 120) {
            return
        }
        Start-Sleep -Seconds 15
    }
    Write-Log "Servicing still active after 60 minutes; relying on the pending restart checks."
}

function Get-PendingReboot {
    $checks = [ordered]@{
        "CBS reboot"            = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending"
        "CBS packages"          = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\PackagesPending"
        "Windows Update reboot" = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired"
        "pending.xml"           = "$env:SystemRoot\WinSxS\pending.xml"
    }
    foreach ($name in $checks.Keys) {
        if (Test-Path -LiteralPath $checks[$name]) { $name }
    }
}

function Get-PendingFileRenames {
    $key = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager"
    (Get-ItemProperty -Path $key -Name PendingFileRenameOperations -ErrorAction SilentlyContinue).PendingFileRenameOperations |
        Where-Object { $_ }
}

function Restart-ForSeal([int]$Restarts, [string]$Reason) {
    @{ restarts = $Restarts + 1 } | ConvertTo-Json | Set-Content -LiteralPath $SealStateFile
    $trigger = New-ScheduledTaskTrigger -AtStartup
    $trigger.Delay = "PT2M"
    Set-ScheduledTask -TaskName "ImageRefreshSeal" -Trigger $trigger | Out-Null
    Write-Log "Restarting ($Reason)."
    Restart-Computer -Force
}

try {
    if (-not (Test-Path -LiteralPath $SealStateFile)) {
        # Phase 1, right after the warm-cache job: remove the runner, then restart so that Sysprep never
        # generalizes a system with servicing operations left by the updates or the build.
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

        Restart-ForSeal -Restarts 0 -Reason "clean boot before Sysprep"
        return
    }

    # Phase 2, at startup: generalize only once servicing is idle and no restart is pending.
    $restarts = [int](Get-Content -LiteralPath $SealStateFile -Raw | ConvertFrom-Json).restarts
    Write-Log "Startup $restarts before Sysprep; waiting for servicing to be idle."
    Wait-ServicingIdle
    Get-PendingFileRenames | ForEach-Object { Write-Log "Pending file rename: $_" }
    $pending = @(Get-PendingReboot)
    if ($pending.Count) {
        if ($restarts -ge $MaxSealRestarts) { throw "A restart is still pending after $restarts restarts: $($pending -join ', ')" }
        Restart-ForSeal -Restarts $restarts -Reason "pending $($pending -join ', ')"
        return
    }
    Write-Log "No restart pending."

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

    # sysprep.exe is a GUI program: the call operator would not wait for it nor set $LASTEXITCODE.
    $sysprep = Start-Process -FilePath "$env:SystemRoot\System32\Sysprep\sysprep.exe" -Wait -PassThru `
        -ArgumentList "/generalize", "/oobe", "/shutdown", "/quiet", "`"/unattend:$UnattendPath`""
    if ($sysprep.ExitCode -ne 0) { throw "Sysprep failed with exit code $($sysprep.ExitCode)." }
}
catch {
    New-Item -Path $InfoDir -ItemType Directory -Force | Out-Null
    Add-Content -Path (Join-Path $InfoDir "seal-error.log") -Value ($_ | Out-String)
    throw
}
