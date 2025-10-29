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
    [string]$WixProjPath,  # Full path to *.wixproj

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

if (-Not (Test-Path $WixProjPath)) {
    Write-Error "*wixproj file not found at path: $WixProjPath"
    exit 1
}

[xml]$wixProjXml = Get-Content $WixProjPath
$propertyGroupNode = $wixProjXml.Project.PropertyGroup | Where-Object { $_.DefineConstants } | Select-Object -First 1

if (-not $propertyGroupNode) {
    Write-Error "No <DefineConstants> found in wixproj — expected exactly one."
    exit 1
}
$defineConstants = $propertyGroupNode.DefineConstants -split ';'
$existingVersion = $defineConstants | Where-Object { $_ -like 'version=*' }

if ($existingVersion) {
    # Update existing Version constant
    $versionConstantIndex = [array]::IndexOf($defineConstants, $existingVersion)
    $defineConstants[$versionConstantIndex] = "version=$versionString"
    Write-Host "Existing Version constant found. Updating to $versionString"
} else {
    # Add new Version constant
    $defineConstants += "version=$versionString"
    Write-Host "No existing Version constant found. Adding Version=$versionString"
}

$propertyGroupNode.DefineConstants = ($defineConstants -join ';')
Write-Host "Updated wixproj Version to $versionString"
$wixProjXml.Save($WixProjPath)
