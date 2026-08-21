<#
 Infomaniak kDrive - Desktop App
 Copyright (C) 2023-2024 Infomaniak Network SA

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

# Source Conan environment to get Qt and other dependency DLLs on PATH
$conanRun = Get-ChildItem -Path build-windows -Recurse -Filter 'conanrun.ps1' -File -ErrorAction SilentlyContinue | Select-Object -First 1
if ($conanRun) {
    Write-Host "Sourcing Conan environment: $($conanRun.FullName)"
    & $conanRun.FullName *> $null
} else {
    Write-Host "Warning: conanrun.ps1 not found, Qt dependencies may not be on PATH." -ForegroundColor Yellow
}

# Set QT_PLUGIN_PATH so Qt can find the offscreen platform plugin
$qtDir = & "$PSScriptRoot\conan\find_conan_dep.ps1" -Package "qt" -BuildDir "build-windows"
if ($qtDir) {
    $env:QT_PLUGIN_PATH = "$qtDir\plugins"
    Write-Host "QT_PLUGIN_PATH set to: $env:QT_PLUGIN_PATH"
} else {
    Write-Host "Warning: Could not locate Qt package, QT_PLUGIN_PATH not set." -ForegroundColor Yellow
}

$testers = Get-ChildItem build-windows -Recurse -Name -Filter 'kDrive_test_*.exe'
$errors = 0
$failures = @()

pushd build-windows\install\bin

$env:PATH = "C:\Program Files (x86)\cppunit\lib;" + $env:PATH

foreach ($file in $testers)
{
    Write-Host "---------- Running $file ----------" -f Yellow
    $path="..\..\$file"

    # Check if the file exists before executing it
    if (-not (Test-Path $path -PathType Leaf)) {
        Write-Host "Error: File $path does not exist." -f Red
        $errors += 1
        $failures+=$file
        continue
    }

    & $path
    if ($LASTEXITCODE -ne 0) {
        $errors += 1
        $failures+=$file
        Write-Host "-------- Failure: $file ($LASTEXITCODE)  --------" -f Red
    }
    else {
        Write-Host "---------- Success: $file ----------" -f Green
    }
}

popd

if ($errors -eq 0) {
    Write-Host "Success: All Tests passed !" -f Green
}
else {
    Write-Host "Failures ($errors): 
    " -f Red
    foreach ($failure in $failures)
    {
        Write-Host "$failure" -f Red
    }
}

exit $errors
