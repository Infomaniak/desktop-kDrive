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

if (-not $env:KDRIVE_TOKEN) { Write-Host "No token found to upload to kDrive." -f Red }
else
{
    $version = (Select-String -Path .\build-windows\build\version.h KDRIVE_VERSION_FULL | foreach-object { $data = $_ -split " "; echo $data[3]})
    $app = "kDrive-$version.exe"
    Set-Location build-windows
    $size = (Get-ChildItem $app | % {[int]($_.length)})
    $mainVersion = $version.Substring(0, 3)
    $minorVersion = $version.Substring(0, 5)

    $uri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$app&directory_path=$mainVersion/$minorVersion&conflict=version"
    $headers = @{
        Authorization="Bearer $env:KDRIVE_TOKEN"
    }

    Invoke-RestMethod -Method "POST" -Uri $uri -Header $headers -ContentType 'application/octet-stream' -InFile $app
}
