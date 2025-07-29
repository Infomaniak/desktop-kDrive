<#
 Infomaniak kDrive - Desktop App
 Copyright (C) 2023-2025 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
#>
param(
    [Parameter(Mandatory=$true)][string]$BuildDir,
    [Parameter(Mandatory=$true)][string]$Package
)

function Log { Write-Host "[INFO] $($args -join ' ')" }
function Err { Write-Error "[ERROR] $($args -join ' ')" ; exit 1 }

$runScript = Get-ChildItem -Path $BuildDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq 'conanrun.ps1' } |
        Select-Object -First 1 -ExpandProperty FullName
if (-not $runScript) {
    Err "Unable to recursively find conanrun.ps1 in '$BuildDir'."
}
& $runScript *> $null

$pathEntries = $env:PATH -split ';'
$conanEntries = $pathEntries | Where-Object { $_ -match '\\.conan2\\p\\' }
if (-not $conanEntries) { Err "No directories in PATH contain '.conan2/p/'." }
$pkgValue = if ($Package.Length -ge 5) { $Package.Substring(0,5) } else { $Package }
$matchingDirs = $conanEntries | Where-Object {
    (Split-Path (Split-Path (Split-Path $_ -Parent) -Parent) -Leaf) -like "$pkgValue*"
}
if (-not $matchingDirs) {
    Err "No directories found in PATH matching the package '$Package' (prefix '$pkgValue')."
}
if ($matchingDirs.Count -gt 1) {
    Err "Multiple directories found in PATH matching the package '$Package' (prefix '$pkgValue'). Please specify a more precise package name."
}

$packageDir = $matchingDirs
Write-Output $packageDir

$deactivateRunScript = Get-ChildItem -Path $BuildDir -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ieq 'deactivate_conanrun.ps1' } |
        Select-Object -First 1 -ExpandProperty FullName
& $deactivateRunScript *> $null
