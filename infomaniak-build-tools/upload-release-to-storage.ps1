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
param (
    [string]$version
)

$app = "kDrive-$version"

$os_s = @(
    "linux",
    "macos",
    "win"
)

$languages = @(
    "de",
    "en",
    "es",
    "fr",
    "it"
)

# Upload release notes
foreach ($os in $os_s)
{
    foreach ($lang in $languages)
    {
        $file = "release_notes\$app\$app-$os-$lang.html"
        $size = (Get-ChildItem $file | % {[int]($_.length)})
        Write-Host "Uploading $file ($size) to the storage" -f Cyan
    }
}

# Upload installers / AppImages
# Windows
$file = "installers\$app.exe"
$size = (Get-ChildItem $file | % {[int]($_.length)})
Write-Host "Uploading $file ($size) to the storage" -f Cyan


