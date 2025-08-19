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
    Usage: infomaniak-build-tools\conan\build_dependencies.ps1 [-Help] [Debug|Release|RelWithDebInfo] [-CI] [-OutputDir <path>]

.PARAMETER BuildType
    Build configuration: Debug (default), Release or RelWithDebInfo.

.PARAMETER Help
    Show this help message.

.PARAMETER CI
    Switch indicating that the script is running on a CI service.

.PARAMETER OutputDir
    Custom output directory for Conan installation. If not provided, defaults to ./build-windows\build in the repo root.

.PARAMETER MakeRelease
    Use the 'infomaniak_release' Conan profile.
#>

param(
    [Parameter(Mandatory = $false, Position = 0, HelpMessage = "Debug, Release or RelWithDebInfo")]
    [ValidateSet("Debug","Release", "RelWithDebInfo")]
    [string]$BuildType = "Debug",

    [Parameter(Mandatory = $false, HelpMessage = "Show help message")]
    [switch]$Help,

    [Parameter(Mandatory = $false, HelpMessage = "Indicate running on CI")]
    [switch]$CI,

    [Parameter(Mandatory = $false, HelpMessage = "Output directory for Conan installation")]
    [string]$OutputDir,

    [Parameter(Mandatory = $false, HelpMessage = "Use the 'infomaniak_release' Conan profile.")]
    [switch]$MakeRelease
)

function Show-Help { Write-Host "Usage: $($MyInvocation.MyCommand.Name) [-Help] [Debug|Release|RelWithDebInfo] [-CI] [-OutputDir <path>] [-MakeRelease]" ; exit 0 }
if ($Help) { Show-Help }

$ErrorActionPreference = "Stop"

function Log { Write-Host "[INFO] $($args -join ' ')" }
function Err { Write-Error "[ERROR] $($args -join ' ')" ; exit 1 }

# Determine repository root and default output directory
$CurrentDir = (Get-Location).Path
$DefaultOutputDir = Join-Path $CurrentDir "build-windows\build"

# If a custom output directory is provided, use it
if ($OutputDir) {
    Log "Using custom output directory: $OutputDir"
} else {
    $OutputDir = $DefaultOutputDir
    Log "No custom output directory provided. Using default: $OutputDir"
}



function Get-ConanExePath {
    try {
        $cmd = Get-Command conan.exe -ErrorAction Stop
        Log "Conan executable found at: $($cmd.Path)"
        return $cmd.Path
    } catch { }

    try {
        $pythonCmd = Get-Command python -ErrorAction Stop

        $venvCheck = & $pythonCmd -c 'import sys; print(sys.prefix != sys.base_prefix)'
        if($venvCheck.Trim().ToLower() -ne "true") {
            Write-Warning "Python virtual environment not activated. It is recommended to install conan with a virtual env."
        }
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

# If we are running in CI mode, we activate the python virtual environment.
if ($CI) {
    # Activate the python virtual environment.
    & "C:\Program Files\Python313\.venv\Scripts\activate.ps1"

    # Call vcvarsall.bat to set up the environment for MSVC
    & "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvars64.bat"
    Log "CI mode enabled."
    $env:SSL_CERT_FILE = (& python -m certifi).Trim() # Since the CI User is the system user (NT AUTHORITY\SYSTEM), we need to set the SSL_CERT_FILE environment variable to the certifi bundle.
    Log "SSL_CERT_FILE set to $($env:SSL_CERT_FILE)"
}

# Locate Conan executable
$ConanExe = Get-ConanExePath
if (-not $ConanExe) {
    Err "Conan executable not found. Please ensure Conan is installed and accessible."
}

function Has-Profile {
    param(
        [string]$ProfileName
    )
    $profiles = & $ConanExe profile list
    return $profiles -match "^\s*$ProfileName\s*$"
}


if (-not (Test-Path -Path "infomaniak-build-tools/conan" -PathType Container)) {
    Err "Please run this script from the repository root."
}

$ConanRemoteBaseFolder = Join-Path $CurrentDir "infomaniak-build-tools/conan"
$LocalRemoteName       = "localrecipes"
$RecipesFolder         = Join-Path $ConanRemoteBaseFolder "recipes"

Log "Current conan home configuration:"
& $ConanExe config home

if ($MakeRelease) {
    $ConanProfile = "infomaniak_release"
    if (-not (Has-Profile -ProfileName $ConanProfile)) {
        Err "Profile '$ConanProfile' does not exist. Please create it."
    }
    $profilePath = & $ConanExe profile path $ConanProfile
    if (-not (Get-Content $profilePath | Select-String -Pattern 'build_type=(Release|RelWithDebInfo)')) {
        Err "Profile '$ConanProfile' must set build_type to Release or RelWithDebInfo."
    }
    if (Get-Content $profilePath | Select-String -Pattern 'tools.cmake.cmaketoolchain:user_toolchain') {
        Err "Profile '$ConanProfile' must not set tools.cmake.cmaketoolchain:user_toolchain."
    }
    Log "Using '$ConanProfile' profile for Conan."
} else {
    $ConanProfile = "default"
    Log "Using '$ConanProfile' profile for Conan."
}

# Define a Conan "Remote" pointing at the on-disk recipe folder.
$remotes = & $ConanExe remote list
if (-not ($remotes -match "^$LocalRemoteName.*\[.*Enabled: True.*\]")) {
    Log "Adding Conan remote '$LocalRemoteName' at '$ConanRemoteBaseFolder'."
    & $ConanExe remote add $LocalRemoteName $ConanRemoteBaseFolder --profile:all="$ConanProfile"
    if ($LASTEXITCODE -ne 0) {
        Err "Failed to add local Conan remote."
    }
} else {
    Log "Conan remote '$LocalRemoteName' already exists and is enabled."
}

# Ensure output directory exists
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null # mkdir

Log "Creating xxHash Conan package..."
& $ConanExe create "$RecipesFolder/xxhash/all/" --build=missing -s:a=build_type=Release --profile:all="$ConanProfile" -r $LocalRemoteName -r conancenter
if ($LASTEXITCODE -ne 0) {
    Err "Failed to create xxHash Conan package."
}

$qt_login_type = if ($CI) { "envvars" } else { "ini" }
& $ConanExe create "$RecipesFolder/qt/all/" --version="6.2.3" --build=missing -s:a=build_type=Release --profile:all="$ConanProfile" -r $LocalRemoteName -r conancenter -o "&:qt_login_type=$qt_login_type"
if ($LASTEXITCODE -ne 0) {
    Err "Failed to create qt Conan package."
}

& $ConanExe create "$RecipesFolder/sentry/all/" --build=missing -s:a=build_type=Release --profile:all="$ConanProfile" -r $LocalRemoteName -r conancenter
if ($LASTEXITCODE -ne 0) {
    Err "Failed to create sentry Conan package."
}

Log "Installing Conan dependencies..."
& $ConanExe install . --output-folder="$OutputDir" --build=missing -s:b=build_type="$BuildType" -s:h=build_type="Release" --profile:all="$ConanProfile" -r $LocalRemoteName -r conancenter -c tools.cmake.cmaketoolchain:generator=Ninja -c tools.env.virtualenv:powershell=powershell -o "qt/*:qt_login_type=$qt_login_type"
if ($LASTEXITCODE -ne 0) {
    Err "Failed to install Conan dependencies."
}

Log "Conan dependencies successfully installed in: $OutputDir"

if ($CI)  {
    # Exit the python virtual environment.
    deactivate
}
