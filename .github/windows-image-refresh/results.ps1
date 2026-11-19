# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2026 Infomaniak Network SA
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

function Get-RefreshOutputs($State, [string]$EventName) {
    if (-not $State.PSObject.Properties["installedUpdates"]) {
        throw "Windows Update state is missing its installed update history."
    }
    if (-not $State.originalVersion -or -not $State.updatedVersion) {
        throw "Windows Update state is missing its Windows versions."
    }

    $updates = @($State.installedUpdates)
    $refreshRequired = $State.stage -eq "ready" -and ($updates.Count -gt 0 -or $EventName -eq "workflow_dispatch")
    return [pscustomobject]@{
        original_version = $State.originalVersion
        updated_version = $State.updatedVersion
        installed_updates = ConvertTo-Json -InputObject $updates -Compress
        refresh_required = $refreshRequired.ToString().ToLowerInvariant()
    }
}

function Format-RefreshNotification($Results, [string]$RunUrl) {
    $outputs = $Results.'warm-cache'.outputs
    $failures = @($Results.PSObject.Properties |
        Where-Object { $_.Value.result -in @("failure", "cancelled") } |
        ForEach-Object { $_.Name })

    if ($failures.Count) {
        $lines = @("**Windows runner image refresh failed**", "Failed jobs: $($failures -join ', ')")
        if ($Results.promote.result -eq "success") {
            $lines += "The new image was promoted, but cleanup failed."
        }
    }
    elseif ($Results.'warm-cache'.result -eq "success" -and $outputs.refresh_required -eq "false") {
        $lines = @("**Windows runner image: no updates**")
    }
    elseif ($Results.promote.result -eq "success") {
        $lines = @("**Windows runner image refreshed**")
    }
    elseif ($Results.validate.result -eq "success" -and $Results.promote.result -eq "skipped") {
        $lines = @("**Windows runner image validated (not promoted)**")
    }
    else {
        throw "Cannot determine the Windows image refresh outcome."
    }

    if ($outputs.original_version -and $outputs.updated_version -and $outputs.installed_updates) {
        # Wrapping this pipeline in @() would nest the JSON array on Windows PowerShell 5.1.
        $updates = $outputs.installed_updates | ConvertFrom-Json
        if ($updates.Count) {
            $lines += @(
                ""
                "Original Windows version: $($outputs.original_version)"
                "Destination Windows version: $($outputs.updated_version)"
                ""
                "Installed updates:"
            )
            $lines += @($updates | ForEach-Object { "- $_" })
        }
        else {
            $lines += @("", "Current Windows version: $($outputs.updated_version)", "No Windows updates were installed.")
        }
    }
    elseif ($failures.Count) {
        $lines += @("", "Windows version and installed updates could not be collected.")
    }
    else {
        throw "Windows image refresh outputs are missing."
    }

    $lines += @("", "[View workflow run]($RunUrl)")
    return $lines -join "`n"
}
