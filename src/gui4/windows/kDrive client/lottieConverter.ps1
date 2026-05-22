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


# ==============================================================================================
# Description:  This PowerShell script converts all `.lottie` animation files within a specified 
#               directory into their `.json` Lottie equivalents.
#
#               Each `.lottie` file is a ZIP-based container that may include multiple animations, 
#               assets, and metadata. Since WinUI 3 currently supports only `.json`-based Lottie 
#               animations, this script automates the extraction of the JSON animation file.
#
#               For each `.lottie` file found:
#                 1. The file is temporarily renamed to `.zip`.
#                 2. The contents are extracted to a temporary directory.
#                 3. The first animation file located in the `animations` subfolder is extracted.
#                 4. The extracted animation is renamed to match the original `.lottie` filename, 
#                    but with a `.json` extension, and placed in the source directory.
#                 5. All temporary files and directories are cleaned up automatically.
#
# Usage:        .\lottieConverter.ps1 -Directory "C:\Path\To\Your\LottieFiles"
#
# Notes:        This script is especially useful for WinUI 3 wich
#               do not yet support the `.lottie` file format directly.
#               kDrive client project automatically run this script as a pre-build event.
# ==============================================================================================

param(
	[Parameter(Mandatory = $true)]
	[string]$Directory
)

# Ensure the directory exists
if (-not (Test-Path $Directory)) {
	Write-Error "Directory '$Directory' does not exist."
	exit 1
}

# Run LottieGen on every .json file in the animations directory
Get-ChildItem -Path $Directory -Filter *.json | ForEach-Object {
	Write-Host "Generating code for '$($_.Name)'..."
	dotnet tool run LottieGen -InputFile $_.FullName -OutputFolder $Directory -Language cs -WinUIVersion 3.0 -GenerateColorBindings -GenerateDependencyObject
}
