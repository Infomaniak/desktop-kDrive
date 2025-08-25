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
    [Parameter(Mandatory = $true)]
    [string]$version
)

if (-not $env:KDRIVE_TOKEN) {
    Write-Host "No KDRIVE_TOKEN found to upload to kDrive." -f Red 
    exit 1
}

if (-not $env:KDRIVE_ID) {
    Write-Host "No KDRIVE_ID found to upload to kDrive." -f Red 
    exit 1
}

if (-not $env:KDRIVE_DIR_ID) {
    Write-Host "No KDRIVE_DIR_ID found to upload to kDrive." -f Red 
    exit 1
}

#version example 3.7.1.20250708
$app = "kDrive-$version"

#Extract the date  (after the 3rd .)
$versionTab = $version.Split('.')
$date = $versionTab[3]
if ($date.Length -ne 8) {
    Write-Host "Invalid version format, expected x.x.x.yyyymmdd, got $version" -f Red
    exit 1
}
$versionNumber = $versionTab[0..2] -join '.'

$headers = @{
    Authorization="Bearer $env:KDRIVE_TOKEN"
}

# upload release notes
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
        if ($size -eq 0) {
            Write-Host "Unable to get file size for $filePath, aborting upload." -f Red
            Pop-Location
            exit 1
        }

        $uri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$fileName&directory_path=$versionNumber/$date/release-notes&conflict=version"
        Write-Host "uploading $filePath to kDrive at $uri"
        $result = Invoke-RestMethod -Method "POST" -Uri $uri -Header $headers -ContentType 'application/octet-stream' -InFile $filePath
        Write-Host "Uploaded $filePath to kDrive successfully. $result" -f Green
        Sleep(5)
    }
}

function Upload-FilesToKDrive {
    param (
        [string]$directory,
        [array]$files,
        [string]$targetSubDir
    )

    Push-Location $directory
    foreach ($file in $files) {
        try {
            $item = Get-Item $file

            # Check if it is a directory (zip it if needed)
            if ($item.PSIsContainer) {
              Write-Host "Zipping directory: $file" -f Yellow

             # Define the ZIP file path: same parent location, same name + .zip
             $parentDir = Split-Path $item.FullName -Parent
             $zipFileName = "$($item.Name).zip"
             $zipFilePath = Join-Path $parentDir $zipFileName

             # Remove existing zip if present
             if (Test-Path $zipFilePath) {
                    Remove-Item $zipFilePath -Force
                 Write-Host "Existing zip removed: $zipFilePath" -f Cyan
              }

              # Load .NET Compression assembly
               Add-Type -AssemblyName System.IO.Compression.FileSystem
            
               # Create ZIP archive beside the folder
               [System.IO.Compression.ZipFile]::CreateFromDirectory($item.FullName, $zipFilePath)
            
               # Replace $file with the zipped file for upload
                $file = $zipFileName            
               Write-Host "Directory zipped: $zipFilePath" -f Green
            }    

            $size = (Get-Item $file).length
            if ($size -eq 0) {
                Write-Host "Unable to get file size for $file, aborting upload." -f Red
                Pop-Location
                exit 1
            }
            $uri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$file&directory_path=$versionNumber/$date/$targetSubDir&conflict=version"
            Write-Host "Uploading $file to kDrive at $uri"
            Invoke-RestMethod -Method "POST" -Uri $uri -Header $headers -ContentType 'application/octet-stream' -InFile $file
            Write-Host "\t\t => ✅" -f Green
        } catch {
            Write-Host "Failed to upload $file to kDrive -> $_" -f Red
            Pop-Location
            exit 1
        }
        Sleep(5)
    }
    Pop-Location
}
Write-Host " - Windows Files - " # Windows
$win_files = @(
    "$app.exe",
    "kDrive.pdb",
    "kDrive_client.pdb",
    "kDrive.src.zip",
    "kDrive_client.src.zip"
)
Upload-FilesToKDrive -directory build-windows -files $win_files -targetSubDir "windows"
Write-Host " - Windows Files - \n"

Write-Host " - macOS Files - " # macOS
$macos_files = @(
    "$app.pkg",
    "$app.zip", # Sparkle zip
    "update-macos-$version.xml", # Sparkle update xml
    "kDrive.dSYM",
    "kDrive_client.dSYM",
    "kDrive.src.zip",
    "kDrive_client.src.zip"
)
Upload-FilesToKDrive -directory build-macos -files $macos_files -targetSubDir "macos"
Write-Host " - macOS Files - \n"

Write-Host " - Linux AMD64 Files - " # Linux AMD
$linux_amd_files = @(
    "$app-amd64.AppImage",
    "kDrive.dbg",
    "kDrive_client.dbg",
    "kDrive.src.zip",
    "kDrive_client.src.zip"
)
Upload-FilesToKDrive -directory build-linux-amd64 -files $linux_amd_files -targetSubDir "linux-amd"
Write-Host " - Linux AMD64 Files - \n"

Write-Host " - Linux ARM64 Files - " # Linux ARM
$linux_arm_files = @(
    "$app-arm64.AppImage",
    "kDrive.dbg",
    "kDrive_client.dbg",
    "kDrive.src.zip",
    "kDrive_client.src.zip"
)
Upload-FilesToKDrive -directory build-linux-arm64 -files $linux_arm_files -targetSubDir "linux-arm"
Write-Host " - Linux ARM64 Files - \n"
