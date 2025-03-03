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

param (
    [string]$dir,
    [string]$tester
)

if (-not $dir) {
    Write-Host "Error: No path provided. Usage: script.ps1 <dir path> <filename>" -ForegroundColor Red
    exit 1
}

if (-not $tester) {
    Write-Host "Error: No filename provided. Usage: script.ps1 <dir path> <filename>" -ForegroundColor Red
    exit 1
}

Write-Host "---------- Running $tester ----------" -ForegroundColor Yellow

Push-Location $dir

if (-not (Test-Path $tester -PathType Leaf)) {
    Write-Host "Error: File $tester does not exist." -ForegroundColor Red
    Pop-Location
    exit 1
}

Start-Process -FilePath ./$tester -NoNewWindow -Wait

if ($LASTEXITCODE -ne 0) {
    Write-Host "---------- Failure: $tester ----------" -ForegroundColor Red
    Pop-Location
    exit 1
} else {
    Write-Host "---------- Success: $tester ----------" -ForegroundColor Green
}

Pop-Location
exit 0
