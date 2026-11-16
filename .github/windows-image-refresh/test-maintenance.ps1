$ErrorActionPreference = "Stop"

# Load only the update-loop functions, never the Cloudbase-Init bootstrap.
$tokens = $null
$parseErrors = $null
$scriptPath = Join-Path $PSScriptRoot "maintenance-userdata.ps1"
$ast = [System.Management.Automation.Language.Parser]::ParseFile($scriptPath, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw "Cannot parse maintenance-userdata.ps1." }
foreach ($name in @("Test-InScopeUpdate", "Invoke-UpdateLoop")) {
    $definition = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if (-not $definition) { throw "Missing function $name." }
    Invoke-Expression $definition.Extent.Text
}

$MaxIterations = 15
$MaxFailuresPerUpdate = 3
$script:currentState = $null
$script:outcome = $null
$script:searchCount = 0
$script:installed = @()
$script:updates = @()
$script:scenario = ""
$script:pendingReboot = $false
$script:restartReason = ""

function Get-State { return $script:currentState }
function Save-State($State) { $script:currentState = $State }
function Write-Log([string]$Message) {}
function Test-RebootPending { return $script:pendingReboot }
function Restart-WhenSafe([string]$Reason) {
    $script:restartReason = $Reason
    throw "Stop simulated reboot"
}
function Complete-Updates($State, [string]$Stage, [string]$Message) {
    # Exit the loop without scheduling a task or rebooting this machine.
    if ($script:outcome) { return }
    $script:outcome = [pscustomobject]@{ stage = $Stage; message = $Message }
    throw "Stop simulated update loop"
}

function New-Object {
    param([string]$ComObject)
    switch ($ComObject) {
        "Microsoft.Update.UpdateColl" {
            $collection = [pscustomobject]@{ items = [System.Collections.ArrayList]::new(); Count = 0 }
            $collection | Add-Member ScriptMethod Add {
                param($update)
                $index = $this.items.Add($update)
                $this.Count = $this.items.Count
                return $index
            }
            $collection | Add-Member ScriptMethod Item { param($index) return $this.items[$index] }
            return $collection
        }
        "Microsoft.Update.Session" {
            $searcher = [pscustomobject]@{}
            $searcher | Add-Member ScriptMethod Search {
                param($query)
                if ($query -ne "IsInstalled=0 and IsHidden=0 and Type='Software'") {
                    throw "Unexpected update query: $query"
                }
                $script:searchCount++
                if ($script:scenario -eq "success" -and $script:searchCount -gt 1) {
                    return [pscustomobject]@{ Updates = @() }
                }
                return [pscustomobject]@{ Updates = $script:updates }
            }
            $downloader = [pscustomobject]@{ Updates = $null }
            $downloader | Add-Member ScriptMethod Download { return $null }
            $installer = [pscustomobject]@{ Updates = $null }
            $installer | Add-Member ScriptMethod Install {
                $script:installed += @($this.Updates.items | ForEach-Object { $_.Title })
                $result = [pscustomobject]@{ RebootRequired = $false }
                $result | Add-Member ScriptMethod GetUpdateResult {
                    param($index)
                    $code = switch ($script:scenario) {
                        "failure" { 4 }
                        "partial" { 3 }
                        default { 2 }
                    }
                    return [pscustomobject]@{ ResultCode = $code; HResult = 0 }
                }
                return $result
            }
            $session = [pscustomobject]@{ ClientApplicationID = "" }
            $session | Add-Member ScriptMethod CreateUpdateSearcher { return $searcher }.GetNewClosure()
            $session | Add-Member ScriptMethod CreateUpdateDownloader { return $downloader }.GetNewClosure()
            $session | Add-Member ScriptMethod CreateUpdateInstaller { return $installer }.GetNewClosure()
            return $session
        }
        default { throw "Unexpected COM object: $ComObject" }
    }
}

function New-FakeUpdate([string]$Title, [int]$Type, [string[]]$Categories) {
    $update = [pscustomobject]@{
        Title = $Title
        Type = $Type
        Categories = @($Categories | ForEach-Object { [pscustomobject]@{ Name = $_ } })
        EulaAccepted = $false
    }
    $update | Add-Member ScriptMethod AcceptEula { $this.EulaAccepted = $true }
    return $update
}

function Start-Scenario([string]$Scenario) {
    $script:scenario = $Scenario
    $script:outcome = $null
    $script:searchCount = 0
    $script:installed = @()
    $script:restartReason = ""
    $script:currentState = [pscustomobject]@{
        stage = "updating"
        iteration = 0
        failures = [pscustomobject]@{}
    }
    try {
        Invoke-UpdateLoop
    }
    catch {
        if ($Scenario -ne "pending" -or $_.Exception.Message -ne "Stop simulated update loop") { throw }
    }
}

$routine = New-FakeUpdate "Security update" 1 @("Security Updates")
$script:updates = @(
    $routine
    (New-FakeUpdate "Preview update" 1 @("Updates"))
    (New-FakeUpdate "Feature update to Windows" 1 @("Updates"))
    (New-FakeUpdate "Upgrade to Windows" 1 @("Updates"))
    (New-FakeUpdate "Driver" 2 @("Drivers"))
    (New-FakeUpdate "Optional feature" 1 @("Feature Packs"))
    (New-FakeUpdate "Version upgrade" 1 @("Upgrades"))
)
Start-Scenario "success"
if ($script:outcome.stage -ne "ready" -or $script:searchCount -ne 2) {
    throw "Routine updates did not converge: $($script:outcome | ConvertTo-Json -Compress)"
}
if ($script:installed.Count -ne 1 -or $script:installed[0] -ne $routine.Title -or -not $routine.EulaAccepted) {
    throw "Non-routine updates were installed or the routine update was not accepted."
}

$script:updates = @(New-FakeUpdate "Failing update" 1 @("Security Updates"))
foreach ($scenario in @("failure", "partial")) {
    Start-Scenario $scenario
    if ($script:outcome.stage -ne "update-failed" -or $script:searchCount -ne $MaxFailuresPerUpdate) {
        throw "Repeated $scenario update results did not stop the update loop."
    }
    if ($script:currentState.failures."Failing update" -ne $MaxFailuresPerUpdate) {
        throw "Update failures were not persisted."
    }
}

$script:updates = @(New-FakeUpdate "Unresolved update" 1 @("Security Updates"))
Start-Scenario "nonconverging"
if ($script:outcome.stage -ne "update-failed" -or $script:searchCount -ne $MaxIterations -or
    $script:outcome.message -notlike "*did not converge*") {
    throw "The update loop did not stop after the iteration limit."
}

$script:pendingReboot = $true
Start-Scenario "pending"
if ($script:restartReason -ne "pending reboot before update scan" -or
    $script:searchCount -ne 0 -or $script:currentState.iteration -ne 0) {
    throw "A pending reboot did not stop the update scan."
}
Write-Host "Windows maintenance simulation passed."
