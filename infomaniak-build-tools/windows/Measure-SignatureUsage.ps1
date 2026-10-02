<#
.SYNOPSIS
    Counts the private-key signatures performed on this machine while a build runs (what per-signature
    providers such as DigiCert KeyLocker bill), without modifying the build or its signtool calls.

.DESCRIPTION
    Uses the "CertInUse" usage telemetry built into Windows' ncrypt.dll (undocumented, disabled by default).
    Once enabled, every NCryptSignHash call that actually produces a signature is reported as event 16 in the
    "Microsoft-Windows-Crypto-NCrypt/CertInUse" log, with the process and a key id (hash of the public key).
      - signtool (called from PowerShell, MSBuild SignAppxPackage, ...) signs through NCryptSignHash, including
        keys stored in legacy CAPI CSPs.
      - One signtool call can produce several signatures (.msix = CodeIntegrity.cat + package = 2): all counted.
      - Signature-size queries and timestamping (no private-key operation) are not counted.
    Start : enables the feature without throttling (HKLM\SYSTEM\CurrentControlSet\Control\Cryptography\CertInUse)
            and enlarges the event log.
    Report: sums the signatures made since Start, per key and per process.
    Stop  : Report + CSV export + restores the original registry and event log settings.
    Start/Report/Stop must run elevated. The build runs unchanged, from any shell, between Start and Stop.
    Validated on Windows 11 build 26200. The feature is undocumented and may behave differently on other builds.

.EXAMPLE
    .\Measure-SignatureUsage.ps1 Start
    # ... run the unchanged build (build-drive.ps1, msbuild, Visual Studio, ...)
    .\Measure-SignatureUsage.ps1 Stop -Thumbprint BBC311586F2629106A67976B135747CEC76982DC
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)]
    [ValidateSet('Start', 'Report', 'Stop')]
    [string]$Action,

    # Certificate(s) whose signatures are totalled. Default: all keys.
    [string[]]$Thumbprint,

    [int]$LogSizeMB = 256,

    [string]$CsvPath = '',

    [string]$StateFile = (Join-Path $env:ProgramData 'Measure-SignatureUsage.state.json')
)

$ErrorActionPreference = 'Stop'
$RegPath = 'HKLM:\SYSTEM\CurrentControlSet\Control\Cryptography\CertInUse'
$Channel = 'Microsoft-Windows-Crypto-NCrypt/CertInUse'

$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an elevated PowerShell (Run as administrator).'
}

# The keyId reported by ncrypt is the first 16 bytes of the SHA-256 of the DER-encoded public key
# (subjectPublicKey content of the certificate, e.g. RSAPublicKey).
function Get-KeyId([Security.Cryptography.X509Certificates.X509Certificate2]$Cert) {
    $hash = [Security.Cryptography.SHA256]::Create().ComputeHash($Cert.PublicKey.EncodedKeyValue.RawData)
    [BitConverter]::ToString($hash, 0, 16).Replace('-', '')
}

function ConvertTo-KeyId([string]$Value) {
    $Value = $Value.ToUpperInvariant()
    if ($Value.Length -gt 32) { $Value.Substring(0, 32) } else { $Value }
}

function Get-CertificateLabels {
    $labels = @{}
    foreach ($cert in @(Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -ErrorAction SilentlyContinue)) {
        try { $id = Get-KeyId $cert } catch { continue }
        $label = '{0} [{1}]' -f $cert.Subject, $cert.Thumbprint
        if (-not $labels.ContainsKey($id)) { $labels[$id] = $label }
        elseif ($labels[$id] -notlike "*$label*") { $labels[$id] += "; $label" }
    }
    $labels
}

function Get-State {
    if (-not (Test-Path $StateFile)) { throw "No measurement in progress: run '$PSCommandPath Start' first." }
    Get-Content $StateFile -Raw | ConvertFrom-Json
}

function Get-SigningEvent([long]$AfterRecordId) {
    $xpath = "*[System[(EventID=16) and (EventRecordID>$AfterRecordId)]]"
    try { $events = @(Get-WinEvent -LogName $Channel -FilterXPath $xpath) }
    catch {
        if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { return }
        throw
    }
    foreach ($e in $events) {
        $json = $e.Properties | ForEach-Object { $_.Value } |
            Where-Object { $_ -is [string] -and $_.TrimStart().StartsWith('{') } | Select-Object -First 1
        if (-not $json) { continue }
        $j = $json | ConvertFrom-Json
        [pscustomobject]@{
            Time      = $e.TimeCreated
            KeyId     = ConvertTo-KeyId $j.keyId
            Provider  = $j.provider
            Process   = $j.procPath
            ProcessId = $e.ProcessId
            ProcStart = [string]$j.procStart
            Sign      = [long]$j.sign
            Decrypt   = [long]$j.decrypt
        }
    }
}

function Show-Report($State, [string]$Csv) {
    $targetIds = @()
    foreach ($tp in @($Thumbprint | ForEach-Object { $_ -split '[,;\s]+' } | Where-Object { $_ })) {
        $tp = ($tp -replace '[^0-9A-Fa-f]', '').ToUpperInvariant()
        $cert = Get-ChildItem "Cert:\CurrentUser\My\$tp", "Cert:\LocalMachine\My\$tp" -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if (-not $cert) {
            $cert = Get-ChildItem Cert:\ -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.Thumbprint -eq $tp } | Select-Object -First 1
        }
        if (-not $cert) { throw "Certificate $tp not found in the certificate stores." }
        $targetIds += Get-KeyId $cert
    }

    $labels = Get-CertificateLabels
    $signing = @(Get-SigningEvent ([long]$State.LastRecordId) | Where-Object { $_.Sign -gt 0 } | ForEach-Object {
            $label = if ($labels.ContainsKey($_.KeyId)) { $labels[$_.KeyId] } else { '' }
            $_ | Add-Member -NotePropertyName Certificate -NotePropertyValue $label -PassThru
        })

    $log = Get-WinEvent -ListLog $Channel
    if ($log.FileSize -ge 0.9 * $log.MaximumSizeInBytes) {
        Write-Warning "The event log is (nearly) full: older events may be overwritten. Use a larger -LogSizeMB."
    }

    Write-Host ("`nSignatures since {0:G}, per key:" -f [datetime]$State.StartTime)
    $signing | Group-Object KeyId | ForEach-Object {
        [pscustomobject]@{
            Signatures  = [long]($_.Group | Measure-Object Sign -Sum).Sum
            Processes   = @($_.Group | ForEach-Object { "$($_.ProcessId)|$($_.ProcStart)" } | Sort-Object -Unique).Count
            Provider    = $_.Group[0].Provider
            Certificate = $_.Group[0].Certificate
            KeyId       = $_.Name
        }
    } | Sort-Object Signatures -Descending | Format-Table -AutoSize -Wrap | Out-String -Width 400 | Write-Host

    $selected = $signing
    if ($targetIds) {
        $selected = @($signing | Where-Object { $targetIds -contains $_.KeyId })
        Write-Host "Selected certificate(s), per process:"
    }
    else { Write-Host "All keys, per process:" }
    $selected | Group-Object Process | ForEach-Object {
        [pscustomobject]@{
            Signatures = [long]($_.Group | Measure-Object Sign -Sum).Sum
            Processes  = @($_.Group | ForEach-Object { "$($_.ProcessId)|$($_.ProcStart)" } | Sort-Object -Unique).Count
            Executable = $_.Name
        }
    } | Sort-Object Signatures -Descending | Format-Table -AutoSize -Wrap | Out-String -Width 400 | Write-Host

    $total = [long]($selected | Measure-Object Sign -Sum).Sum
    Write-Host "TOTAL signatures: $total" -ForegroundColor Green
    if ($Csv) {
        $signing | Export-Csv -Path $Csv -NoTypeInformation -Encoding UTF8
        Write-Host "Per-event details: $Csv"
    }
}

function Start-Measurement {
    if (Test-Path $StateFile) { throw "A measurement is already in progress ($StateFile): run '$PSCommandPath Stop' first." }
    $key = Get-Item $RegPath -ErrorAction SilentlyContinue
    $log = Get-WinEvent -ListLog $Channel
    $newest = Get-WinEvent -LogName $Channel -MaxEvents 1 -ErrorAction SilentlyContinue
    [ordered]@{
        StartTime       = (Get-Date).ToString('o')
        LastRecordId    = if ($newest) { $newest.RecordId } else { 0 }
        RegKeyExisted   = [bool]$key
        ClientEnable    = if ($key) { $key.GetValue('ClientEnable') } else { $null }
        ThrottleMinutes = if ($key) { $key.GetValue('ClientSideEventThrottlingMinutes') } else { $null }
        LogMaxSize      = $log.MaximumSizeInBytes
        LogEnabled      = $log.IsEnabled
    } | ConvertTo-Json | Set-Content -Path $StateFile -Encoding UTF8

    if ($log.MaximumSizeInBytes -lt $LogSizeMB * 1MB) { $log.MaximumSizeInBytes = [long]$LogSizeMB * 1MB }
    $log.IsEnabled = $true
    $log.SaveChanges()
    if (-not $key) { New-Item -Path $RegPath -Force | Out-Null }
    New-ItemProperty -Path $RegPath -Name ClientEnable -PropertyType DWord -Value 1 -Force | Out-Null
    # 0 = one event per signature (the default 60 min throttling loses the last ops of short-lived processes)
    New-ItemProperty -Path $RegPath -Name ClientSideEventThrottlingMinutes -PropertyType DWord -Value 0 -Force | Out-Null
    Write-Host "Signature counting enabled. Run the build now, then: $PSCommandPath Stop [-Thumbprint <thumbprint>]"
}

function Stop-Measurement {
    $state = Get-State
    # Restore the settings even if the report fails (certificate not found, CSV file locked, ...).
    try { Show-Report $state $CsvPath }
    finally {
        if ($state.RegKeyExisted) {
            foreach ($value in @(@{ Name = 'ClientEnable'; Data = $state.ClientEnable },
                    @{ Name = 'ClientSideEventThrottlingMinutes'; Data = $state.ThrottleMinutes })) {
                if ($null -eq $value.Data) { Remove-ItemProperty -Path $RegPath -Name $value.Name -ErrorAction SilentlyContinue }
                else { New-ItemProperty -Path $RegPath -Name $value.Name -PropertyType DWord -Value $value.Data -Force | Out-Null }
            }
        }
        else { Remove-Item -Path $RegPath -Recurse -Force -ErrorAction SilentlyContinue }

        $log = Get-WinEvent -ListLog $Channel
        $log.IsEnabled = [bool]$state.LogEnabled
        $log.MaximumSizeInBytes = [long]$state.LogMaxSize
        try { $log.SaveChanges() }
        catch { Write-Warning "Event log size not restored ($($_.Exception.Message)). Clear the log to shrink it." }
        Remove-Item $StateFile
        Write-Host 'Original CertInUse and event log settings restored.'
    }
}

switch ($Action) {
    'Start' { Start-Measurement }
    'Report' { Show-Report (Get-State) }
    'Stop' { Stop-Measurement }
}
