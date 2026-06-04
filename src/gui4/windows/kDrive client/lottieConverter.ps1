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
# Description:  This PowerShell script converts all `.json` Lottie animation files within a 
#               specified directory into C# code using LottieGen.
#
#               The script includes hash-based change detection to avoid unnecessary regeneration:
#                 1. Calculates SHA256 hashes of all `.json` files in the directory.
#                 2. Compares the combined hash with the previous run (stored in `.lottie-hashes`).
#                 3. Only regenerates C# files if the hashes differ.
#                 4. Saves the new hash for future comparisons.
#
#               For each `.json` file found:
#                 - Generates C# code using LottieGen with WinUI 3.0 target.
#                 - Enables color bindings and dependency object generation.
#
# Usage:        .\lottieConverter.ps1 -Directory "C:\Path\To\Your\LottieFiles"
#
# Notes:        This script is especially useful for WinUI 3 projects.
#               kDrive client project automatically runs this script as a pre-build event.
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

# Define the hash file path
$hashFilePath = Join-Path $Directory ".lottie-hashes"

# Get all .json files in the directory
$jsonFiles = Get-ChildItem -Path $Directory -Filter *.json | Sort-Object Name

if ($jsonFiles.Count -eq 0) {
	Write-Host "No .json files found in directory '$Directory'."
	exit 0
}

# Calculate combined hash of all .json files
$combinedHash = ""
foreach ($file in $jsonFiles) {
	$fileHash = (Get-FileHash -Path $file.FullName -Algorithm SHA256).Hash
	$combinedHash += "$($file.Name):$fileHash`n"
}
$currentHash = (Get-FileHash -InputStream ([System.IO.MemoryStream]::new([System.Text.Encoding]::UTF8.GetBytes($combinedHash))) -Algorithm SHA256).Hash

# Check if hash file exists and compare
$shouldRegenerate = $true
if (Test-Path $hashFilePath) {
	$previousHash = Get-Content $hashFilePath -Raw
	if ($previousHash.Trim() -eq $currentHash) {
		Write-Host "No changes detected in Lottie files. Skipping regeneration."
		$shouldRegenerate = $false
	}
}

# Only regenerate if hashes differ
if ($shouldRegenerate) {
	Write-Host "Changes detected in Lottie files. Regenerating C# code..."

	# Run LottieGen on every .json file in the animations directory
	foreach ($file in $jsonFiles) {
		Write-Host "Generating code for '$($file.Name)'..."
		dotnet tool run LottieGen -InputFile $file.FullName -OutputFolder $Directory -Language cs -WinUIVersion 3.0 -GenerateColorBindings -GenerateDependencyObject
	}

	# Save the new hash
	Set-Content -Path $hashFilePath -Value $currentHash -NoNewline
	Write-Host "Successfully generated C# code for $($jsonFiles.Count) file(s)."
} else {
	Write-Host "All generated files are up to date."
}
