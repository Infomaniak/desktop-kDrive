#!/usr/bin/env pwsh
#
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

# ----------------------------------------------------------------------
# This script is used to build the dependencies of the kDrive client.
# It will use conan to install the dependencies.

<#
.SYNOPSIS
    Infomaniak kDrive Desktop – build dependencies via Conan (Windows only)

.DESCRIPTION
    Usage: infomaniak-build-tools\conan\build_dependencies.ps1 [-Help] [Debug|Release|RelWithDebInfo] [-CI] [-OutputDir <path>] [-MakeRelease] [-CleanCache] [-Update]

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

.PARAMETER Update
    Ask Conan to check remotes for newer versions/revisions.
    Disabled by default to keep CI deterministic and avoid local recipe revision/timestamp conflicts.
#>

param(
    [Parameter(Mandatory = $false, Position = 0, HelpMessage = "Debug, Release or RelWithDebInfo")]
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildType = "Debug",

    [Parameter(Mandatory = $false, HelpMessage = "Show help message")]
    [switch]$Help,

    [Parameter(Mandatory = $false, HelpMessage = "Indicate running on CI")]
    [switch]$CI,

    [Parameter(Mandatory = $false, HelpMessage = "Output directory for Conan installation")]
    [string]$OutputDir,

    [Parameter(Mandatory = $false, HelpMessage = "Use the 'infomaniak_release' Conan profile.")]
    [switch]$MakeRelease,

    [Parameter(Mandatory = $false, HelpMessage = "Update environment variables after installation.")]
    [switch]$UpdateEnvironment,

    [Parameter(Mandatory = $false, HelpMessage = "Clean the Conan cache after installation to save disk space.")]
    [switch]$CleanCache,

    [Parameter(Mandatory = $false, HelpMessage = "Ask Conan to check remotes for newer versions/revisions.")]
    [switch]$Update
)

function Show-Help
{
    Write-Host "Usage: $( $MyInvocation.MyCommand.Name ) [-Help] [Debug|Release|RelWithDebInfo] [-CI] [-OutputDir <path>] [-MakeRelease] [-CleanCache] [-Update]"; exit 0
}
if ($Help)
{
    Show-Help
}

$ErrorActionPreference = "Stop"

function Log
{
    Write-Host "[INFO] $( $args -join ' ' )"
}
function Err
{
    Write-Error "[ERROR] $( $args -join ' ' )"; exit 1
}

# Determine repository root and default output directory
$CurrentDir = (Get-Location).Path
$DefaultOutputDir = Join-Path $CurrentDir "build-windows"

# If a custom output directory is provided, use it
if ($OutputDir)
{
    Log "Using custom output directory: $OutputDir"
}
else
{
    $OutputDir = $DefaultOutputDir
    Log "No custom output directory provided. Using default: $OutputDir"
}

# Remove previous CMakeUserPresets.json if it exists
if (Test-Path -Path ".\CMakeUserPresets.json")
{
    Log "Removing previous CMakeUserPresets.json file."
    Remove-Item -Path ".\CMakeUserPresets.json" -Force
}



function Get-ConanExePath
{
    try
    {
        $cmd = Get-Command conan.exe -ErrorAction Stop
        Log "Conan executable found at: $( $cmd.Path )"
        return $cmd.Path
    }
    catch
    {
    }

    try
    {
        $pythonCmd = Get-Command python -ErrorAction Stop

        $venvCheck = & $pythonCmd -c 'import sys; print(sys.prefix != sys.base_prefix)'
        if ($venvCheck.Trim().ToLower() -ne "true")
        {
            Write-Warning "Python virtual environment not activated. It is recommended to install conan with a virtual env."
        }
    }
    catch
    {
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

    $rawPath = & $pythonCmd.Path -c $pythonCode 2> $null
    $exePath = $rawPath.Trim()
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exePath))
    {
        Err "Unable to locate 'conan.exe' via Python."
        return $null
    }

    Log "Conan executable found at: $exePath"
    return $exePath
}

# If we are running in CI mode, we activate the python virtual environment.
if ($CI)
{
    # Activate the python virtual environment.
    & "C:\Program Files\Python313\.venv\Scripts\activate.ps1"

    # Call vcvarsall.bat to set up the environment for MSVC
    & "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
    Log "CI mode enabled."
    $env:SSL_CERT_FILE = (& python -m certifi).Trim() # Because of the CI User on Windows, we need to set the SSL_CERT_FILE environment variable to the certifi bundle.
    Log "SSL_CERT_FILE set to $( $env:SSL_CERT_FILE )"
}

# Locate Conan executable
$ConanExe = Get-ConanExePath
if (-not $ConanExe)
{
    Err "Conan executable not found. Please ensure Conan is installed and accessible."
}

function Has-Profile
{
    param(
        [string]$ProfileName
    )
    $profiles = & $ConanExe profile list
    return $profiles -match "^\s*$ProfileName\s*$"
}


if (-not (Test-Path -Path "infomaniak-build-tools/conan" -PathType Container))
{
    Err "Please run this script from the repository root."
}

$ConanRemoteBaseFolder = Join-Path $CurrentDir "infomaniak-build-tools/conan"
$LocalRemoteName = "localrecipes"

Log "Current conan home configuration:"
& $ConanExe config home

if ($MakeRelease)
{
    $ConanProfile = "infomaniak_release"
    if (-not (Has-Profile -ProfileName $ConanProfile))
    {
        Err "Profile '$ConanProfile' does not exist. Please create it."
    }
    $profilePath = & $ConanExe profile path $ConanProfile
    if (-not (Get-Content $profilePath | Select-String -Pattern 'build_type=(Release|RelWithDebInfo)'))
    {
        Err "Profile '$ConanProfile' must set build_type to Release or RelWithDebInfo."
    }
    if (Get-Content $profilePath | Select-String -Pattern 'tools.cmake.cmaketoolchain:user_toolchain')
    {
        Err "Profile '$ConanProfile' must not set tools.cmake.cmaketoolchain:user_toolchain."
    }
    Log "Using '$ConanProfile' profile for Conan."
}
else
{
    $ConanProfile = "default"
    Log "Using '$ConanProfile' profile for Conan."
}

# Define a Conan "Remote" pointing at the on-disk recipe folder.
$NormalizeRemoteUrl = {
    param([string]$Url)
    if ( [string]::IsNullOrWhiteSpace($Url))
    {
        return ""
    }
    return ($Url -replace '\\', '/').TrimEnd('/')
}

$ExpectedRemoteUrl = try
{
    (Resolve-Path -Path $ConanRemoteBaseFolder).Path
}
catch
{
    $ConanRemoteBaseFolder
}
$ExpectedRemoteUrlNormalized = & $NormalizeRemoteUrl $ExpectedRemoteUrl

$remotesJson = & $ConanExe remote list --format=json
if ($LASTEXITCODE -ne 0)
{
    Err "Failed to list Conan remotes."
}

try
{
    $remotes = $remotesJson | ConvertFrom-Json
}
catch
{
    Err "Failed to parse Conan remotes JSON output."
}

$matchingRemote = $remotes | Where-Object {
    $_.name -eq $LocalRemoteName -and
            $_.enabled -eq $true -and
            (& $NormalizeRemoteUrl $_.url) -eq $ExpectedRemoteUrlNormalized
} | Select-Object -First 1

if ($matchingRemote)
{
    Log "Conan remote '$LocalRemoteName' already exists, has the expected URL and is enabled."
}
else
{
    $remoteWithSameName = $remotes | Where-Object { $_.name -eq $LocalRemoteName } | Select-Object -First 1
    if ($remoteWithSameName)
    {
        Log "Removing Conan remote '$LocalRemoteName' because URL/enabled state does not match the expected configuration."
        & $ConanExe remote remove $LocalRemoteName
        if ($LASTEXITCODE -ne 0)
        {
            Err "Failed to remove existing Conan remote '$LocalRemoteName'."
        }
    }

    Log "Adding Conan remote '$LocalRemoteName' at '$ConanRemoteBaseFolder'."
    & $ConanExe remote add $LocalRemoteName $ConanRemoteBaseFolder
    if ($LASTEXITCODE -ne 0)
    {
        Err "Failed to add local Conan remote."
    }
}

# Ensure output directory exists
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null # mkdir

Log "Installing Conan dependencies..."
$conanInstallArgs = @(
    "install", ".",
    "--output-folder=$OutputDir",
    "--build=missing",
    "-s:a=build_type=$BuildType",
    "--profile:all=$ConanProfile",
    "-r", $LocalRemoteName,
    "-r", "conancenter",
    "-c", "tools.cmake.cmaketoolchain:generator=Ninja",
    "-c", "tools.env.virtualenv:powershell=powershell"
)
if ($CI)
{
    $conanInstallArgs += "-o"
    $conanInstallArgs += "qt/*:qt_login_type=envvars"
}
if ($Update)
{
    $conanInstallArgs += "--update"
}
& $ConanExe @conanInstallArgs
if ($LASTEXITCODE -ne 0)
{
    Err "Failed to install Conan dependencies."
}

Log "Conan dependencies successfully installed in: $OutputDir"

#clear old conan PATH entries
Log "Removing previous conan path in user Path environment variable..."
$currentUserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
$newUserPath = ($currentUserPath -split ';' | Where-Object { $_ -notlike "*\.conan2\*" }) -join ';'
[System.Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
Log "previous conan path entries removed."

if ($CleanCache)
{
    Log "Cleaning Conan cache to save disk space..."
    & $ConanExe cache clean --source --build --temp "*"
    Log "Conan cache cleaned."
}

# Update user environment variables if requested (programs will need to be restarted to see the changes)
if ($UpdateEnvironment)
{
    Log "Adding new conan path to user Path environment variable..."
    $currentUserPath = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $allowedNames = @("build", "bin")

    $conanRoot = Join-Path $env:USERPROFILE ".conan2\p\b"
    function Get-ConanBinaries {
        param([string]$Path)
        foreach ($item in Get-ChildItem -Path $Path -Force) {

            # Skip unwanted Conan cache subtree early
            if ($item.PSIsContainer -and $item.FullName -match '\\.conan2\\p\\b\\[^\\]+\\b\\') {
                Log "Skipping Conan cache subtree: $($item.FullName)"
                continue
            } elseif ($item.PSIsContainer -and $item.Name -eq "bin") {
                Log "Registering bin directory: '$($item.FullName)'"
                return $item.FullName
            } elseif ($item.PSIsContainer) {
                Get-ConanBinaries -Path $item.FullName
            }
        }
    }

    Log "Starting Conan binary scan from root: $conanRoot"

    $conanBinPaths = Get-ConanBinaries $conanRoot | Sort-Object -Unique

    Log "Completed Conan scan. Found $($conanBinPaths.Count) unique binary directories."

    foreach ($path in $conanBinPaths)
    {
        if (-not ($newUserPath -split ';' | Where-Object { $_ -eq $path }))
        {
            Log "Adding '$path' to user Path."
            $newUserPath += ";$path"
        }
    }
    [System.Environment]::SetEnvironmentVariable("Path", $newUserPath, "User")
    Log "New user Path set to: $newUserPath"
    Log "User Path environment variable updated. Please restart your programs to apply the changes."
}

if ($CI)
{
    # Exit the python virtual environment.
    deactivate
}
