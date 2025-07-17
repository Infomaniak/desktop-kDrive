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

$testers = Get-ChildItem build-windows -Recurse -Name -Filter 'kDrive_test_*.exe'
$errors = 0
$failures = @()

$find_dep_script = "infomaniak-build-tools\conan\find_conan_dep.ps1"
if (-not (Test-Path $find_dep_script)) {
    Write-Host "Error: Cannot find $find_dep_script" -f Red
    exit 1
}
$cppunit_args = @{
    Package = "cppunit"
    Version = "1.15.1"
    Ci = $true
}
$cppunit_folder = & $find_dep_script @cppunit_args
$oldPath = $env:PATH
$env:PATH = "$($cppunit_folder)\bin;$env:PATH"


pushd build-windows\install\bin

foreach ($file in $testers)
{
    Write-Host "---------- Running $file ----------" -f Yellow
    $path="..\..\$file"
    & $path
    if ($LASTEXITCODE -ne 0) {
        $errors += 1
        $failures+=$file
        Write-Host "---------- Failure: $file ----------" -f Red
    }
    else {
        Write-Host "---------- Success: $file ----------" -f Green
    }
}

popd
$env:PATH = $oldPath # Restore original PATH

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
