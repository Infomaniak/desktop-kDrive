#!/usr/bin/env pwsh
#
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

# ----------------------------------------------------------------------
# This script is used to build the dependencies of the kDrive client.
# It will use conan to install the dependencies.

<#
.SYNOPSIS
    Infomaniak kDrive Desktop – build dependencies via Conan (Windows only)

.DESCRIPTION
    Usage: infomaniak-build-tools\conan\build_dependencies.ps1 [-Help] [Debug|Release]

.PARAMETER BuildType
    Build configuration: Debug (default) or Release.

.PARAMETER Help
    Show this help message.
#>

param(
    [Parameter(Mandatory = $false, Position = 0, HelpMessage = "Debug or Release")]
    [ValidateSet("Debug","Release")]
    [string]$BuildType = "Debug",

    [Parameter(Mandatory = $false, HelpMessage = "Show help message")]
    [switch]$Help
)

function Show-Help { Write-Host "Usage: $($MyInvocation.MyCommand.Name) [-Help] [Debug|Release]" ; exit 0 }
if ($Help) { Show-Help }

$ErrorActionPreference = "Stop"

function Log { Write-Host "[INFO] $($args -join ' ')" }
function Err { Write-Error "[ERROR] $($args -join ' ')" ; exit 1 }


if (-not (Test-Path -Path "infomaniak-build-tools/conan" -PathType Container)) {
    Err "Please run this script from the repository root."
}

if (-not (Get-Command conan -ErrorAction SilentlyContinue)) {
    Err "Conan is not installed. Please install Conan first."
}

$CurrentDir            = (Get-Location).Path
$ConanRemoteBaseFolder = Join-Path $CurrentDir "infomaniak-build-tools/conan"
$LocalRemoteName       = "localrecipes"
$RecipesFolder         = Join-Path $ConanRemoteBaseFolder "recipes"

# Create local remote for local conan recipes
$remotes = conan remote list
if (! $remotes -match "^$LocalRemoteName.*\[.*Enabled: True.*\]") {
    Log "Adding local Conan remote."
    conan remote add $LocalRemoteName $ConanRemoteBaseFolder | Out-Null
} else {
    Log "Local Conan remote already exists."
}

# Output folder
$BaseOutput = Join-Path $CurrentDir "build-windows\conan"
if ($BuildType -eq "Debug") {
    Log "Selected Debug build."
    $OutputDir = Join-Path $BaseOutput "Debug"
} else {
    Log "Selected Release build."
    $OutputDir = Join-Path $BaseOutput "Release"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null # mkdir

Log "Creating xxHash Conan package..."
# Here we build xxhash with Release build type as said in `infomaniak-build-tools/windows/Readme.md`
conan create "$RecipesFolder/xxhash/all/" --build=missing -s build_type=Release -r $LocalRemoteName

Log "Installing Conan dependencies..."
conan install . --output-folder="$OutputDir" --build=missing -s build_type=$BuildType -r $LocalRemoteName

Log "Conan dependencies successfully installed in: $OutputDir"