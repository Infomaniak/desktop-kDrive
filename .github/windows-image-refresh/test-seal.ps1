$ErrorActionPreference = "Stop"

# Load only the idle check, never the sealing or Sysprep operations.
$tokens = $null
$parseErrors = $null
$scriptPath = Join-Path $PSScriptRoot "seal.ps1"
$ast = [System.Management.Automation.Language.Parser]::ParseFile($scriptPath, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Cannot parse seal.ps1." }
$definition = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq "Wait-RunnerIdle"
}, $true)
if (-not $definition) { throw "Missing runner idle check." }
Invoke-Expression $definition.Extent.Text

$script:seconds = 0
$script:scenario = ""
function Get-Date { return ([DateTime]"2026-01-01").AddSeconds($script:seconds) }
function Start-Sleep([int]$Seconds) { $script:seconds += $Seconds }
function Get-Process {
    param([string]$Name, [string]$ErrorAction)
    if ($Name -ne "Runner.Worker") { throw "Unexpected process name: $Name" }
    if ($script:scenario -eq "busy" -or
        ($script:scenario -eq "intermittent" -and $script:seconds -ge 40 -and $script:seconds -lt 50)) {
        return [pscustomobject]@{ Name = $Name }
    }
}

$script:scenario = "idle"
Wait-RunnerIdle
if ($script:seconds -ne 60) { throw "Sealing did not wait for a full minute of inactivity." }

$script:seconds = 0
$script:scenario = "intermittent"
Wait-RunnerIdle
if ($script:seconds -ne 110) { throw "Worker activity did not reset the idle deadline." }

$script:seconds = 0
$script:scenario = "busy"
$timedOut = $false
try {
    Wait-RunnerIdle
}
catch {
    if ($_.Exception.Message -ne "Runner did not become idle before the sealing deadline.") { throw }
    $timedOut = $true
}
if (-not $timedOut -or $script:seconds -ne 1800) { throw "Busy runner did not time out." }
Write-Host "Sealing idle-check simulation passed."
