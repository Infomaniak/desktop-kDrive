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
    [string]$WixProjPath,  # Full path to *.wixproj

    [Parameter(Mandatory = $true)]
    [string]$RepositoryRootPath
)


function Get-Overlay-Guid {
    param (
        [string] $path,
        [string] $keyType
    )

    try {
        $guid = (Select-String -Path "$path\extensions\windows\standard\KDOverlays\OverlayConstants.h" $keyType |  Select-Object -ExpandProperty Line)
        $guid = $guid.Split(" ")[-1] # Extract last word.
        $guid = $guid.Substring(2, $guid.length - 3) # Trim the initial 'L' character and the surrounding double quotes.
    } catch {
            Write-Error "Failed to retrieve overlay GUID for '$keyType' from OverlayConstants.h"
            exit 1
    }
    
    return $guid
}

function Update-Define-Constants {
    param (
        [string] $key,
        [string] $value,
        [ref] $defineConstants
    )

    $pattern = "$key=*"
    $existing = $defineConstants.Value | Where-Object { $_ -like $pattern }

    if ($existing) {
        # Update existing constant
        $constantIndex = [array]::IndexOf($defineConstants.Value, $existing)
        $defineConstants.Value[$constantIndex] = "$key=$value"
        Write-Host "Existing '$key' constant found. Updating to '$value'."
    } else {
        # Add new constant
        $defineConstants.Value += "$key=$value"
        Write-Host "No existing '$key' constant found. Adding '$key=$value'"
    }
}


$versionString = ""
$RepositoryRootPath = $RepositoryRootPath.Trim('"')
$RepositoryRootPath = Resolve-Path $RepositoryRootPath 

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

$defineConstants = @()
if (-not $propertyGroupNode) {
    Write-Host "No <DefineConstants> found in wixproj. This is consistent with the default template."
} else {
    $defineConstants = $propertyGroupNode.DefineConstants -split ';'
}

# Version

Update-Define-Constants -Key "version" -Value $versionString -DefineConstants ([ref]$defineConstants)

# Repository root path

Update-Define-Constants -Key "desktop-kDriveDir" -Value $RepositoryRootPath -DefineConstants ([ref]$defineConstants)

# Overlay GUIDs 

$guid = Get-Overlay-Guid -Path $RepositoryRootPath -KeyType "OVERLAY_GUID_OK"
Update-Define-Constants -Key "regKeyOkGUID" -Value $guid -DefineConstants ([ref]$defineConstants)

$guid = Get-Overlay-Guid -Path $RepositoryRootPath -KeyType "OVERLAY_GUID_ERROR"
Update-Define-Constants -Key "regKeyErrorGUID" -Value $guid -DefineConstants ([ref]$defineConstants)

$guid = Get-Overlay-Guid -Path $RepositoryRootPath -KeyType "OVERLAY_GUID_SYNC"
Update-Define-Constants -Key "regKeySyncGUID" -Value $guid -DefineConstants ([ref]$defineConstants)

$guid = Get-Overlay-Guid -Path $RepositoryRootPath -KeyType "OVERLAY_GUID_WARNING"
Update-Define-Constants -Key "regKeyWarningGUID" -Value $guid -DefineConstants ([ref]$defineConstants)

# Edit

if (-not $propertyGroupNode) {
    $defineConstantsNode = $wixProjXml.CreateElement("DefineConstants")
    $defineConstantsNode.InnerText = ($defineConstants -join ';')
    $propertyGroupNode = $wixProjXml.Project.PropertyGroup | Select-Object -First 1
    $propertyGroupNode.AppendChild($defineConstantsNode)
} else {
    $propertyGroupNode.DefineConstants = ($defineConstants -join ';')
}

# Save

Write-Host "Updated wixproj Version to $versionString"
$wixProjXml.Save($WixProjPath)
