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

Set-Location $RunnerDir

& .\config.cmd `
    --url $RepoUrl `
    --token $Token `
    --labels $Labels `
    --name $RunnerName `
    --unattended `
    --runasservice `
    --replace

if ($LASTEXITCODE -eq 0) {
    New-Item -Path $MarkerFile -ItemType File -Force | Out-Null
} else {
    Write-Error "config.cmd failed with exit code $LASTEXITCODE"
}
