#ps1_sysnative
$ErrorActionPreference = "Stop"

$RunnerDir  = "C:\actions-runner"
$MarkerFile = Join-Path $RunnerDir ".registered"

$RepoUrl    = "__REPO_URL__"
$Token      = "__REG_TOKEN__"
$Labels     = "__TAGS__"
$RunnerName = "$env:COMPUTERNAME"

# Local Administrator account (consistent with the AutoLogon set in the unattend file).
$AdminAccount = "Administrateur"

# --- Register the runner (no service, no logon-account flags) ---
if (-not (Test-Path $MarkerFile)) {
    Set-Location $RunnerDir

    & .\config.cmd `
        --url $RepoUrl `
        --token $Token `
        --labels $Labels `
        --name $RunnerName `
        --unattended `
        --replace

    if ($LASTEXITCODE -ne 0) {
        Write-Error "config.cmd failed with exit code $LASTEXITCODE"
        exit 1
    }

    New-Item -Path $MarkerFile -ItemType File -Force | Out-Null
}

# --- Run interactively as Admin at logon, instead of as a service ---
$TaskName  = "ActionsRunnerStart"
$Action    = New-ScheduledTaskAction -Execute "$RunnerDir\run.cmd" -WorkingDirectory $RunnerDir
$Trigger   = New-ScheduledTaskTrigger -AtLogOn -User $AdminAccount
$Principal = New-ScheduledTaskPrincipal -UserId $AdminAccount -LogonType Interactive -RunLevel Highest

Register-ScheduledTask -TaskName $TaskName -Action $Action -Trigger $Trigger `
    -Principal $Principal -Force | Out-Null

# If Admin already has an interactive session open at the time this script runs,
# start the runner immediately rather than waiting for the next logon.
#
# IMPORTANT: this user-data script is executed by cloudbase-init under its own
# (cloud-init) identity. Calling Start-Process here would launch run.cmd as the
# cloud-init account. Instead we start the scheduled task, which always runs
# under its configured principal ($AdminAccount, RunLevel Highest), so the
# runner correctly inherits the Administrator identity.
$adminSession = Get-Process -Name explorer -IncludeUserName -ErrorAction SilentlyContinue |
    Where-Object { $_.UserName -like "*$AdminAccount" }

# --- Allow execution of .ps1 scripts (required by the runner to run job scripts) ---
# The runner invokes temporary .ps1 files (e.g. from _work\_temp). Without a
# permissive execution policy these fail with:
#   "... cannot be loaded because running scripts is disabled on this system."
# Set the policy for both the LocalMachine and CurrentUser scopes so the runner
# (and any interactive session) can execute scripts.
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope LocalMachine -Force
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser -Force

if ($adminSession) {
    Start-ScheduledTask -TaskName $TaskName
}