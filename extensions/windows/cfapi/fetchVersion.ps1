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
param(
    [Parameter(Mandatory = $true)]
    [string]$ManifestPath,  # Full path to Package.appxmanifest

    [Parameter(Mandatory = $true)]
    [string]$RepositoryRootPath
)


$versionString = ""
$RepositoryRootPath = $RepositoryRootPath.Trim('"')
. "$RepositoryRootPath\infomaniak-build-tools\version-helpers.ps1"
$versionString = Get-VersionFromJson -RepositoryRootPath $RepositoryRootPath -IncludeBuildVersion $true
if($versionString -eq "") {
    Write-Error "Failed to retrieve version string from JSON"
    exit 1
}

if (-Not (Test-Path $ManifestPath)) {
    Write-Error "Manifest file not found at path: $ManifestPath"
    exit 1
}

[xml]$manifestXml = Get-Content $ManifestPath
$identityNode = $manifestXml.Package.Identity
if (-not $identityNode) {
    Write-Error "No <Identity> node found in appxmanifest"
    exit 1
}

$identityNode.Version = $versionString
Write-Host "Updated appmanifest Version to $versionString"

$manifestXml.Save($ManifestPath)
