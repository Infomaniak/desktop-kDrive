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
param(
    [Parameter(Mandatory = $true)]
    [string]$CsProjPath,  # Full path to *.csproj

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

if (-Not (Test-Path $CsProjPath)) {
    Write-Error "*.csproj file not found at path: $CsProjPath"
    exit 1
}

[xml]$CsProjXml = Get-Content $CsProjPath

$propertyGroupNode = $CsProjXml.SelectSingleNode(
    "/Project/PropertyGroup[not(@Condition)]"
)

if (-not $propertyGroupNode) {
    Write-Error "No suitable <PropertyGroup> found."
    exit 1
}

$assemblyVersionNode = $propertyGroupNode.SelectSingleNode("AssemblyVersion")

if (-not $assemblyVersionNode) {
    $assemblyVersionNode = $CsProjXml.CreateElement("AssemblyVersion")
    $propertyGroupNode.AppendChild($assemblyVersionNode) | Out-Null
}

# SAFE: always XmlElement
$assemblyVersionNode.InnerText = $versionString

$CsProjXml.Save($CsProjPath)

Write-Output "Updated AssemblyVersion to $versionString in $CsProjPath"

