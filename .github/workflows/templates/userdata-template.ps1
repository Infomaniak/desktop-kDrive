#ps1_sysnative
$ErrorActionPreference = "Stop"

$RunnerDir  = "C:\actions-runner"
$MarkerFile = Join-Path $RunnerDir ".registered"

if (Test-Path $MarkerFile) {
    Write-Output "Runner already registered, skipping."
    exit 0
}

$RepoUrl    = "__REPO_URL__"
$Token      = "__REG_TOKEN__"
$Labels     = "__TAGS__"
$RunnerName = "$env:COMPUTERNAME"
$ServicePassword = "__REG_ADMIN_PASS__"

# Local Administrator account (consistent with the AutoLogon set in the unattend file).
# The ".\" prefix explicitly targets a local account rather than a domain account.
$ServiceAccount  = ".\Administrator"
$ServicePassword = "Passw0rd"

Set-Location $RunnerDir

& .\config.cmd `
    --url $RepoUrl `
    --token $Token `
    --labels $Labels `
    --name $RunnerName `
    --unattended `
    --runasservice `
    --windowslogonaccount $ServiceAccount `
    --windowslogonpassword $ServicePassword `
    --replace

if ($LASTEXITCODE -eq 0) {
    # Confirm the service was actually created with the expected account
    $service = Get-CimInstance Win32_Service -Filter "Name LIKE '%actions.runner%'" -ErrorAction Stop
    Write-Output "Service: $($service.Name) | Account: $($service.StartName) | State: $($service.State)"

    if ($service.State -ne "Running") {
        Write-Error "Service is not running after configuration. Current state: $($service.State)"
    }

    New-Item -Path $MarkerFile -ItemType File -Force | Out-Null
} else {
    Write-Error "config.cmd failed with exit code $LASTEXITCODE"
}