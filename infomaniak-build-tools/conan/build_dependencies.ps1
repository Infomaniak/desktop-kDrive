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
    Usage: infomaniak-build-tools\conan\build_dependencies.ps1 [-Help] [Debug|Release|RelWithDebInfo] [-CI]

.PARAMETER BuildType
    Build configuration: Debug (default), Release or RelWithDebInfo.

.PARAMETER Help
    Show this help message.

.PARAMETER CI
    Switch indicating that the script is running on a CI service.
#>

param(
    [Parameter(Mandatory = $false, Position = 0, HelpMessage = "Debug, Release or RelWithDebInfo")]
    [ValidateSet("Debug","Release", "RelWithDebInfo")]
    [string]$BuildType = "Debug",

    [Parameter(Mandatory = $false, HelpMessage = "Show help message")]
    [switch]$Help,

    [Parameter(Mandatory = $false, HelpMessage = "Indicate running on CI")]
    [switch]$CI
)

function Show-Help { Write-Host "Usage: $($MyInvocation.MyCommand.Name) [-Help] [Debug|Release|RelWithDebInfo] [-CI]" ; exit 0 }
if ($Help) { Show-Help }

$ErrorActionPreference = "Stop"

function Log { Write-Host "[INFO] $($args -join ' ')" }
function Err { Write-Error "[ERROR] $($args -join ' ')" ; exit 1 }

if(!$CI) {
    Err "This script is only ready for the CI. Please perform the installation manually."
}

function Get-ConanExePath {
    try {
        $cmd = Get-Command conan.exe -ErrorAction Stop
        Log "Conan executable found at: $($cmd.Path)"
        return $cmd.Path
    } catch { }

    try {
        $pythonCmd = Get-Command python -ErrorAction Stop
    } catch {
        Err "Interpreter 'python' not found. Please install Python 3 and/or add it to the PATH."
        return $null
    }

    $pythonCode = @"
import os, sys
try:
    import conans
except ImportError:
    sys.exit(1)
modpath = conans.__file__
prefix = modpath.split(os.sep + 'Lib' + os.sep)[0]
exe = os.path.join(prefix, 'Scripts', 'conan.exe')
print(exe)
"@

    $rawPath = & $pythonCmd.Path -c $pythonCode 2>$null
    $exePath = $rawPath.Trim()
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exePath)) {
        Err "Unable to locate 'conan.exe' via Python."
        return $null
    }

    Log "Conan executable found at: $exePath"
    return $exePath
}

# If we are running in CI mode, set $ConanProfile
$ConanProfileParam = ""
if ($CI) {
    $ConanProfileParam = "--profile C:\ProgramData\.conan2\profiles\default"

    # Activate the python virtual environment.
    & "C:\Program Files\Python313\.venv\Scripts\activate.ps1"

    Log "CI mode enabled. Conan user home set to: $env:CONAN_USER_HOME"
}

# Locate Conan executable
$ConanExe = Get-ConanExePath
if (-not $ConanExe) {
    Err "Conan executable not found. Please ensure Conan is installed and accessible."
}

if (-not (Test-Path -Path "infomaniak-build-tools/conan" -PathType Container)) {
    Err "Please run this script from the repository root."
}

$CurrentDir            = (Get-Location).Path
$ConanRemoteBaseFolder = Join-Path $CurrentDir "infomaniak-build-tools/conan"
$LocalRemoteName       = "localrecipes"
$RecipesFolder         = Join-Path $ConanRemoteBaseFolder "recipes"

# Create local remote for local Conan recipes
$remotes = & $ConanExe remote list
if (-not ($remotes -match "^$LocalRemoteName.*\[.*Enabled: True.*\]")) {
    Log "Adding local Conan remote."
    & $ConanExe remote add $LocalRemoteName $ConanRemoteBaseFolder $ConanProfileParam | Out-Null
} else {
    Log "Local Conan remote already exists."
}

# Output folder
$OutputDir = Join-Path $CurrentDir "build-windows\build"

New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null # mkdir

Log "Creating xxHash Conan package..."
& $ConanExe create "$RecipesFolder/xxhash/all/" --build=missing -s build_type=Release -r $LocalRemoteName $ConanProfileParam

Log "Installing Conan dependencies..."
& $ConanExe install . --output-folder="$OutputDir" --build=missing -s build_type=$BuildType -r $LocalRemoteName $ConanProfileParam

Log "Conan dependencies successfully installed in: $OutputDir"

if ($CI)  {
    # Exit the python virtual environment.
    deactivate
}