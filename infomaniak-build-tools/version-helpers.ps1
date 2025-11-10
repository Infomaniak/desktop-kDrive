<#
# Infomaniak kDrive - Desktop
# Copyright (C) 2023-2025 Infomaniak Network SA
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
#>

#Relative path to version.json from repository root
$VersionJsonRelativePath = "version.json"

function Get-VersionFromJson {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepositoryRootPath, # Full path to repository root directory, the script will deduce the version.json path from this

        [Parameter()]
        [bool]$IncludeBuildVersion = $true
    )
    $VersionJsonPath = Join-Path -Path $RepositoryRootPath -ChildPath $VersionJsonRelativePath

    # --- Step 1: Check and parse JSON file ---
    if (-Not (Test-Path $VersionJsonPath)) {
        throw "Version JSON file not found at path: $VersionJsonPath"
    }

    $versionData = Get-Content $VersionJsonPath -Raw | ConvertFrom-Json
    $version = $versionData.Version

    if (-not $version) {
        throw "Version object not found in JSON"
    }

    # --- Step 2: Build version string ---
    $versionString = "$($version.major).$($version.minor).$($version.patch)"
    if ($IncludeBuildVersion) {
        $versionString += ".$($version.build)"
    }

    return $versionString
}
