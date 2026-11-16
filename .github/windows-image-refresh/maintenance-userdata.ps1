#ps1_sysnative
# Cloudbase-Init user-data for the Windows image refresh maintenance VM.
# Rendered by .github/workflows/update-windows-image.yml (placeholders __XXX__).
#
# First boot (run by Cloudbase-Init): record the original Windows version, configure autologon,
# register an isolated GitHub runner (unique label only, no default labels) WITHOUT starting it,
# then hand over to a SYSTEM startup task that installs routine Windows updates across reboots.
# Once updates converge (or fail), the runner is started so the "warm-cache" job can report the outcome.
param([switch]$Resume)

$ErrorActionPreference = "Stop"

$StateDir = "C:\ImageRefresh"
$StateFile = Join-Path $StateDir "state.json"
$LogFile = Join-Path $StateDir "refresh.log"
$ResumeScript = Join-Path $StateDir "refresh.ps1"
$ResumeTaskName = "ImageRefreshResume"
$RunnerTaskName = "ActionsRunnerStart"
$RunnerDir = "C:\actions-runner"
$AdminAccount = "Administrator"
$MaxIterations = 15
$MaxFailuresPerUpdate = 3

function Write-Log([string]$Message) {
    $line = "$(Get-Date -Format o) $Message"
    Add-Content -Path $LogFile -Value $line
    Write-Host $line
}

function Get-State {
    return Get-Content -Path $StateFile -Raw | ConvertFrom-Json
}

function Save-State($State) {
    $State | ConvertTo-Json -Depth 5 | Set-Content -Path $StateFile -Encoding UTF8
}

function Get-WindowsVersionString {
    $cv = Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion"
    return "$($cv.CurrentMajorVersionNumber).$($cv.CurrentMinorVersionNumber).$($cv.CurrentBuild).$($cv.UBR)"
}

function Test-RebootPending {
    if ((New-Object -ComObject Microsoft.Update.SystemInfo).RebootRequired) { return $true }
    if (Test-Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Component Based Servicing\RebootPending") { return $true }
    if (Test-Path "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\WindowsUpdate\Auto Update\RebootRequired") { return $true }
    return $false
}

function Restart-WhenSafe([string]$Reason) {
    # Do not reboot while Cloudbase-Init is still running its plugins: it would not record their completion.
    $deadline = (Get-Date).AddMinutes(15)
    while ((Get-Date) -lt $deadline) {
        $svc = Get-Service -Name "cloudbase-init" -ErrorAction SilentlyContinue
        $proc = Get-Process -Name "cloudbase-init" -ErrorAction SilentlyContinue
        if ((-not $svc -or $svc.Status -ne "Running") -and -not $proc) {
            Write-Log "Rebooting: $Reason"
            & shutdown.exe /r /t 30 /c "kDrive image refresh: $Reason" /d p:2:17
            if ($LASTEXITCODE -ne 0) { throw "Failed to schedule reboot (exit code $LASTEXITCODE)." }
            exit 0
        }
        Start-Sleep -Seconds 15
    }
    throw "Cloudbase-Init did not finish before the reboot deadline."
}

function Test-InScopeUpdate($Update) {
    # Routine updates only: no feature upgrades, no drivers, no preview releases.
    if ($Update.Type -ne 1) { return $false }
    if ($Update.Title -match "Preview|Feature update|Upgrade to Windows") { return $false }
    foreach ($category in $Update.Categories) {
        if ($category.Name -in @("Upgrades", "Drivers", "Feature Packs")) { return $false }
    }
    return $true
}

function Complete-Updates($State, [string]$Stage, [string]$Message) {
    $State.stage = $Stage
    $State.message = $Message
    $State.updatedVersion = Get-WindowsVersionString
    $State.finished = (Get-Date).ToUniversalTime().ToString("o")
    Save-State $State
    Write-Log "Update stage finished with '$Stage': $Message (Windows $($State.updatedVersion))"

    # Start the isolated runner at the next interactive Administrator logon (autologon), as in normal CI images.
    # It also starts on failure so that the warm-cache job reports the failure and its log.
    $action = New-ScheduledTaskAction -Execute "$RunnerDir\run.cmd" -WorkingDirectory $RunnerDir
    $trigger = New-ScheduledTaskTrigger -AtLogOn -User $AdminAccount
    $principal = New-ScheduledTaskPrincipal -UserId $AdminAccount -LogonType Interactive -RunLevel Highest
    Register-ScheduledTask -TaskName $RunnerTaskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
    Unregister-ScheduledTask -TaskName $ResumeTaskName -Confirm:$false -ErrorAction SilentlyContinue

    Restart-WhenSafe "start maintenance runner"
}

function Invoke-UpdateLoop {
    $state = Get-State
    if ($state.stage -ne "updating") {
        Write-Log "Nothing to do in stage '$($state.stage)'."
        return
    }

    try {
        while ($true) {
            if (Test-RebootPending) {
                Restart-WhenSafe "pending reboot before update scan"
            }

            if ($state.iteration -ge $MaxIterations) {
                Complete-Updates $state "update-failed" "Updates did not converge after $MaxIterations iterations."
            }

            $state.iteration++
            Save-State $state
            Write-Log "Update iteration $($state.iteration): searching for routine updates."

            $session = New-Object -ComObject Microsoft.Update.Session
            $session.ClientApplicationID = "kDrive image refresh"
            $searcher = $session.CreateUpdateSearcher()
            $result = $searcher.Search("IsInstalled=0 and IsHidden=0 and Type='Software'")

            $toInstall = New-Object -ComObject Microsoft.Update.UpdateColl
            foreach ($update in $result.Updates) {
                if (Test-InScopeUpdate $update) {
                    if (-not $update.EulaAccepted) { $update.AcceptEula() }
                    [void]$toInstall.Add($update)
                    Write-Log "  Selected: $($update.Title)"
                }
                else {
                    Write-Log "  Skipped (out of scope): $($update.Title)"
                }
            }

            if ($toInstall.Count -eq 0) {
                Complete-Updates $state "ready" "No routine update left to install."
            }

            $downloader = $session.CreateUpdateDownloader()
            $downloader.Updates = $toInstall
            [void]$downloader.Download()

            $installer = $session.CreateUpdateInstaller()
            $installer.Updates = $toInstall
            $installResult = $installer.Install()

            $failures = @{}
            if ($state.failures) {
                $state.failures.PSObject.Properties | ForEach-Object { $failures[$_.Name] = [int]$_.Value }
            }
            for ($i = 0; $i -lt $toInstall.Count; $i++) {
                $title = $toInstall.Item($i).Title
                $updateResult = $installResult.GetUpdateResult($i)
                Write-Log "  Result for '$title': code=$($updateResult.ResultCode) hresult=$($updateResult.HResult)"
                # Only a fully successful update is accepted.
                if ($updateResult.ResultCode -ne 2) {
                    $failures[$title] = 1 + [int]$failures[$title]
                    if ($failures[$title] -ge $MaxFailuresPerUpdate) {
                        $state.failures = [pscustomobject]$failures
                        Complete-Updates $state "update-failed" "'$title' failed $MaxFailuresPerUpdate times (hresult $($updateResult.HResult))."
                    }
                }
            }
            $state.failures = [pscustomobject]$failures
            Save-State $state

            if ($installResult.RebootRequired -or (Test-RebootPending)) {
                Restart-WhenSafe "updates installed (iteration $($state.iteration))"
            }
        }
    }
    catch {
        Write-Log "Unexpected error: $($_ | Out-String)"
        Complete-Updates $state "update-failed" "Unexpected error: $($_.Exception.Message)"
    }
}

if ($Resume) {
    Invoke-UpdateLoop
    exit 0
}

# --- First boot ---
$RepoUrl = "__REPO_URL__"
$Token = "__REG_TOKEN__"
$Label = "__LABEL__"
$RunnerName = "__REG_NAME__"
$AdminPassB64 = "__ADMIN_PASS_B64__"
$AdminPass = [System.Text.Encoding]::UTF8.GetString([System.Convert]::FromBase64String($AdminPassB64))

New-Item -Path $StateDir -ItemType Directory -Force | Out-Null
if (Test-Path $StateFile) {
    Write-Log "State already initialized; resuming."
    Invoke-UpdateLoop
    exit 0
}

$state = [pscustomobject]@{
    stage           = "updating"
    runnerName      = $RunnerName
    originalVersion = Get-WindowsVersionString
    updatedVersion  = ""
    iteration       = 0
    failures        = [pscustomobject]@{}
    message         = ""
    started         = (Get-Date).ToUniversalTime().ToString("o")
    finished        = ""
}
Save-State $state
Write-Log "Original Windows version: $($state.originalVersion)"

$winlogonKey = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Winlogon"
Set-ItemProperty -Path $winlogonKey -Name AutoAdminLogon -Value "1"
Set-ItemProperty -Path $winlogonKey -Name DefaultUserName -Value $AdminAccount
Set-ItemProperty -Path $winlogonKey -Name DefaultPassword -Value $AdminPass

Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine -Force

# Register now (the registration token expires after one hour), but only start the runner after updates.
# No default labels: the runner must never pick jobs targeting [self-hosted, Windows].
Push-Location $RunnerDir
& .\config.cmd --url $RepoUrl --token $Token --labels $Label --no-default-labels --name $RunnerName --unattended --replace
$configExitCode = $LASTEXITCODE
Pop-Location
if ($configExitCode -ne 0) {
    Write-Log "config.cmd failed with exit code $configExitCode"
    exit 1
}

# Keep a copy without secrets for the startup task.
(Get-Content -Path $PSCommandPath -Raw).Replace($Token, "REDACTED").Replace($AdminPassB64, "REDACTED") |
    Set-Content -Path $ResumeScript -Encoding UTF8

$action = New-ScheduledTaskAction -Execute "powershell.exe" `
    -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$ResumeScript`" -Resume"
$trigger = New-ScheduledTaskTrigger -AtStartup
$principal = New-ScheduledTaskPrincipal -UserId "SYSTEM" -LogonType ServiceAccount -RunLevel Highest
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Hours 6) -AllowStartIfOnBatteries
Register-ScheduledTask -TaskName $ResumeTaskName -Action $action -Trigger $trigger -Principal $principal `
    -Settings $settings -Force | Out-Null

# Run the update loop outside Cloudbase-Init so its reboots never interrupt Cloudbase-Init plugins.
Start-ScheduledTask -TaskName $ResumeTaskName
Write-Log "Update loop started."
exit 0
