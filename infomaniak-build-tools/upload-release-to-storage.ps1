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
    [string]$version,
    [string]$user,
    [string]$pass
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
try {
$proc = Start-Process -NoNewWindow -FilePath "./mc.exe" -ArgumentList "alias set kdrive-storage https://storage5.infomaniak.com $user $pass --api s3v4" -Wait -PassThru
if (-not ($proc.ExitCode -eq 0)) {
    Write-Host "❌ config host add kdrive-storage failed: $proc" -f Red
    exit 1
}
# Upload release notes
foreach ($os in $os_s)
{
    foreach ($lang in $languages)
    {
        $fileName = "$app-$os-$lang.html"
        $filePath = ".\release_notes\$app\$fileName"
        if (-not (Test-Path $filePath)) {
            Write-Host "❌ File $filePath does not exist, aborting upload." -f Red
            exit 1
        }

        $size = (Get-ChildItem $filePath | % {[int]($_.length)})
        $proc = Start-Process -NoNewWindow -FilePath "./mc.exe" `
        -ArgumentList "cp --attr Content-Type=text/html $filePath kdrive-storage/download/drive/desktopclient/$fileName" `
        -Wait -PassThru

        if (-not ($proc.ExitCode -eq 0)) {
            Write-Host "❌ Upload of $fileName failed" -f Red
            exit 1
        }
    }
}

# Upload the application files
$executables = @(
    "$app.exe",
    "$app-amd64.AppImage"
)

foreach ($executable in $executables)
{
    $filePath = ".\installers\$executable"
        if (-not (Test-Path $filePath)) {
            Write-Host "❌ File $filePath does not exist, aborting upload." -f Red
            exit 1
        }
    $size = (Get-ChildItem $filePath | % {[int]($_.length)})
    $proc = Start-Process -NoNewWindow -FilePath "./mc.exe" `
    -ArgumentList "cp --attr Content-Type=application/octet-stream $filePath kdrive-storage/download/drive/desktopclient/$executable --debug" `
    -Wait -PassThru

    if (-not ($proc.ExitCode -eq 0)) {
        Write-Host "❌ Upload of $executable failed" -f Red
        exit 1
    }
}

Write-Host "✅ All files uploaded successfully to kDrive storage." -f Green
} catch {
    Write-Host "❌ An error occurred: $_" -f Red
    exit 1
} finally {
    # Clean up the connection to the storage
    Start-Process -NoNewWindow -FilePath "./mc.exe" -ArgumentList "alias remove kdrive-storage" -Wait
}



