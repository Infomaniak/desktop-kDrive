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
    # Le service est créé par config.cmd sous NT AUTHORITY\NETWORK SERVICE par défaut,
    # ce qui cause l'erreur 1068 au boot (permissions insuffisantes / init réseau).
    # On le bascule explicitement en LocalSystem.
    $service = Get-Service "GitHub Actions Runner*" -ErrorAction Stop

    Write-Output "Service found: $($service.Name). Reconfiguring to run as LocalSystem..."

    # Syntaxe sc.exe : espace obligatoire après le signe '='
    $scResult = sc.exe config $service.Name obj= "LocalSystem"
    Write-Output $scResult

    Start-Service -Name $service.Name

    # Vérification que le service tourne bien après le changement de compte
    Start-Sleep -Seconds 3
    $status = (Get-Service -Name $service.Name).Status
    Write-Output "Service status after reconfiguration: $status"

    if ($status -ne "Running") {
        Write-Error "Service failed to start after switching to LocalSystem."
    }

    New-Item -Path $MarkerFile -ItemType File -Force | Out-Null
} else {
    Write-Error "config.cmd failed with exit code $LASTEXITCODE"
}