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
    [string]$version,

    [ValidateSet('win', 'macos', 'linux-arm', 'linux-amd')]
    [string] $os,

    [switch] $test
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

if (-not $env:KDRIVE_ORGA_ID) {
    Write-Host "No KDRIVE_ORGA_ID found to upload recovery updater link." -f Red
    exit 1
}

# version example 3.7.1.0
$app = "kDrive-$version"

# Extract the build number  (after the 3rd .)
$versionTab = $version.Split('.')
$buildNumber = $versionTab[3]

# version number example: 3.7.1
$versionNumber = $versionTab[0..2] -join '.'

# Full version number example: 3.7.1.1
$fullVersionNumber = $versionTab[0..3] -join '.'

$headers = @{
    Authorization="Bearer $env:KDRIVE_TOKEN"
}

# upload release notes
$languages = @(
    "de",
    "en",
    "es",
    "fr",
    "it"
)

$simplifiedOs = $os
if ($simplifiedOs -eq "linux-arm" -Or $simplifiedOs -eq "linux-amd") {
    $simplifiedOs = "linux"
}

foreach ($lang in $languages)
{
    $fileName = "kDrive-$versionNumber-$simplifiedOs-$lang.html"

    $filePath = ".\release_notes\kDrive-$versionNumber\$fileName"
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

    $uri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$fileName&directory_path=$versionNumber/$buildNumber/release-notes&conflict=version"
    Write-Host "uploading $filePath to kDrive at $uri"
    $result = Invoke-RestMethod -Method "POST" -Uri $uri -Header $headers -ContentType 'application/octet-stream' -InFile $filePath
    Write-Host "Uploaded $filePath to kDrive successfully. $result" -f Green
    Sleep(5)

    # Upload legacy file name as well so that older versions (pre 3.7.5) can retrieve the latest release notes
    $legacyFileName = "kDrive-$fullVersionNumber-$simplifiedOs-$lang.html"
    $uri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$legacyFileName&directory_path=$versionNumber/$buildNumber/release-notes&conflict=version"
    Write-Host "uploading $filePath to kDrive at $uri"
    $result = Invoke-RestMethod -Method "POST" -Uri $uri -Header $headers -ContentType 'application/octet-stream' -InFile $filePath
    Write-Host "Uploaded $filePath to kDrive successfully. $result" -f Green
    Sleep(5)
}

function Upload-FilesToKDrive {
    param (
        [string]$directory,
        [array]$files,
        [string]$targetSubDir
    )

    $uploadedFileIds = @{}

    Push-Location $directory
    foreach ($fileEntry in $files) {
        $file = $fileEntry
        $isMandatory = $true

        if ($fileEntry -is [array]) {
            $file = [string]$fileEntry[0]
            if ($fileEntry.Count -ge 2) {
                $isMandatory = [bool]$fileEntry[1]
            }
        }

        try {
            if (-not (Test-Path $file)) {
                $message = if ($isMandatory) { "❌ File $file does not exist, aborting upload." } else { "⚠ Optional file $file does not exist, skipping upload." }
                $color = if ($isMandatory) { 'Red' } else { 'Yellow' }
                Write-Host $message -f $color
                if ($isMandatory) {
                    Pop-Location
                    exit 1
                }
                continue
            }

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
            $directoryPath = "$versionNumber/$buildNumber/$targetSubDir"

            if ($test) {
                $directoryPath = "Test/$directoryPath"
            }

            $uri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$file&directory_path=$directoryPath&conflict=version"           
            Write-Host "Uploading $file to kDrive at $uri"
            $response = Invoke-RestMethod -Method "POST" -Uri $uri -Header $headers -ContentType 'application/octet-stream' -InFile $file
            Write-Host "\t\t => ✅" -f Green

            if ($response.data -and $response.data.id -and $response.data.parent_id) {
                $uploadedFileIds[$file] = @{
                    id = $response.data.id
                    parentId = $response.data.parent_id
                }
            } else {
                Write-Host "Upload succeeded but response is missing id/parent_id for $file -> $response" -f Red
                Pop-Location
                exit 1
            }
        } catch {
            if ($isMandatory) {
                Write-Host "Failed to upload $file to kDrive -> $_" -f Red
                Pop-Location
                exit 1
            }

            Write-Host "Warning: failed to upload optional file $file to kDrive -> $_" -f Yellow
        }
        Sleep(5)
    }
    Pop-Location

    return $uploadedFileIds
}

function Compare-Versions {
    param (
        [string]$versionA,
        [string]$versionB
    )

    $partsA = $versionA.Split('.')
    $partsB = $versionB.Split('.')
    $maxLen = [Math]::Max($partsA.Count, $partsB.Count)

    for ($i = 0; $i -lt $maxLen; $i++) {
        $a = if ($i -lt $partsA.Count) { [int]$partsA[$i] } else { 0 }
        $b = if ($i -lt $partsB.Count) { [int]$partsB[$i] } else { 0 }
        if ($a -gt $b) { return 1 }
        if ($a -lt $b) { return -1 }
    }
    return 0
}

function Upload-RecoveryUpdaterLink {
    param (
        [string]$fileId,
        [string]$parentId,
        [string]$osName,
        [string]$fullVersion
    )

    $kSuiteUrl = "https://ksuite.infomaniak.com/$env:KDRIVE_ORGA_ID/kdrive/app/drive/$env:KDRIVE_ID/files/$parentId/preview/unknown/$fileId"
    $urlFileName = "kDriveRecoveryUpdater-$fullVersion-$osName.url"
    $linkDirPath = "kDriveRecoveryUpdater"
    if ($test) {
        $linkDirPath = "Test/$linkDirPath"
    }

    $shouldUpload = $true
    $filesToDelete = @()

    try {
        $searchUri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/files/search/default?query=kDriveRecoveryUpdater-&with=path&order_by=relevance"
        $searchResponse = Invoke-RestMethod -Method "GET" -Uri $searchUri -Header $headers

        if ($searchResponse.data) {
            $existingFiles = $searchResponse.data | Where-Object {
                $_.name -like "kDriveRecoveryUpdater-*-$osName.url" -and
                $_.path -like "*$linkDirPath*"
            }
            if ($existingFiles) {
                foreach ($existingFile in $existingFiles) {
                    $versionMatch = [regex]::Match($existingFile.name, "kDriveRecoveryUpdater-(.+)-$osName\.url")
                    if ($versionMatch.Success) {
                        $existingVersion = $versionMatch.Groups[1].Value
                        Write-Host "Existing recovery updater link points to version $existingVersion, new version is $fullVersion"
                        $cmp = Compare-Versions -versionA $fullVersion -versionB $existingVersion
                        if ($cmp -lt 0) {
                            Write-Host "New version ($fullVersion) is lower than existing ($existingVersion). Skipping link update." -f Yellow
                            $shouldUpload = $false
                        } elseif ($cmp -gt 0 -and $existingFile.id) {
                            $filesToDelete += [pscustomobject]@{ id = $existingFile.id; version = $existingVersion; name = $existingFile.name }
                        }
                    }
                }
            }
        }
    } catch {
       Write-Host "Could not check existing recovery updater link; aborting link update -> $_" -f Red
       return
    }

    if (-not $shouldUpload) {
        return
    }

    $urlLines = @("[InternetShortcut]", "URL=$kSuiteUrl")
    $tempLinkPath = Join-Path $env:TEMP $urlFileName
    Set-Content -Path $tempLinkPath -Value $urlLines -Encoding UTF8

    try {
        $size = (Get-Item $tempLinkPath).length
        $uploadUri = "https://api.infomaniak.com/3/drive/$env:KDRIVE_ID/upload?directory_id=$env:KDRIVE_DIR_ID&total_size=$size&file_name=$urlFileName&directory_path=$linkDirPath&conflict=version"
        Write-Host "Uploading recovery updater link $urlFileName to kDrive at $uploadUri"
        Invoke-RestMethod -Method "POST" -Uri $uploadUri -Header $headers -ContentType 'application/octet-stream' -InFile $tempLinkPath
        Write-Host "Recovery updater link uploaded => ✅" -f Green

        foreach ($oldFile in $filesToDelete) {
            try {
                $deleteUri = "https://api.infomaniak.com/2/drive/$env:KDRIVE_ID/files/$($oldFile.id)"
                Invoke-RestMethod -Method "DELETE" -Uri $deleteUri -Header $headers -ContentType 'application/json'
                Write-Host "Deleted older recovery updater link $($oldFile.name) (v$($oldFile.version)) => ✅" -f Green
            } catch {
                Write-Host "Warning: failed to delete older recovery updater link $($oldFile.name) -> $_" -f Yellow
            }
        }
    } catch {
        Write-Host "Warning: failed to upload recovery updater link -> $_" -f Yellow
    } finally {
        if (Test-Path $tempLinkPath) {
            Remove-Item $tempLinkPath -Force
        }
    }
}

if ($os -eq "win") {
    Write-Host " - Windows Files - " # Windows
    $win_files = @(
        @("$app.exe", $true),
        @("$app.exe.sha256", $true),
        @("$app.msi", $false),
        @("kDrive.pdb", $true),
        @("kDrive_client.pdb", $true),
        @("kDrive.src.zip", $true),
        @("kDrive_client.src.zip", $false),
        @("kDriveRecoveryUpdater-$version.exe", $true),
        @("kDriveRecoveryUpdater-$version.exe.sha256", $true)
    )
    $uploadedIds = Upload-FilesToKDrive -directory build-windows -files $win_files -targetSubDir "windows"

    $recoveryFileName = "kDriveRecoveryUpdater-$version.exe"
    if ($uploadedIds.ContainsKey($recoveryFileName)) {
        $fileInfo = $uploadedIds[$recoveryFileName]
        Upload-RecoveryUpdaterLink -fileId $fileInfo.id -parentId $fileInfo.parentId -osName "windows" -fullVersion $version
    }
    Write-Host " - Windows Files - \n"
}

if ($os -eq "macos") {
    Write-Host " - macOS Files - " # macOS
    $macos_files = @(
        @("$app.pkg", $true),
        @("$app.pkg.sha256", $true),
        @("$app.zip", $true), # Sparkle zip
        @("update-macos-$version.xml", $true), # Sparkle update xml
        @("kDrive.dSYM", $true),
        @("kDrive_client.dSYM", $true),
        @("kDrive.src.zip", $true),
        @("kDrive_client.src.zip", $true),
        @("kDriveRecoveryUpdater-$version.zip", $true),
        @("kDriveRecoveryUpdater-$version.zip.sha256", $true)
    )
    $uploadedIds = Upload-FilesToKDrive -directory build-macos -files $macos_files -targetSubDir "macos"

    $recoveryFileName = "kDriveRecoveryUpdater-$version.zip"
    if ($uploadedIds.ContainsKey($recoveryFileName)) {
        $fileInfo = $uploadedIds[$recoveryFileName]
        Upload-RecoveryUpdaterLink -fileId $fileInfo.id -parentId $fileInfo.parentId -osName "macos" -fullVersion $version
    }
    Write-Host " - macOS Files - \n"
}

if ($os -eq "linux-amd") {
    Write-Host " - Linux AMD64 Files - " # Linux AMD
    $linux_amd_files = @(
        @("$app-amd64.AppImage", $true),
        @("$app-amd64.AppImage.sha256", $true),
        @("kDrive.dbg", $true),
        @("kDrive_client.dbg", $true),
        @("kDrive.src.zip", $true),
        @("kDrive_client.src.zip", $true),
        @("kDriveRecoveryUpdater-$version-amd64.AppImage", $true),
        @("kDriveRecoveryUpdater-$version-amd64.AppImage.sha256", $true)
    )
    $uploadedIds = Upload-FilesToKDrive -directory build-linux-amd64 -files $linux_amd_files -targetSubDir "linux-amd"

    $recoveryFileName = "kDriveRecoveryUpdater-$version-amd64.AppImage"
    if ($uploadedIds.ContainsKey($recoveryFileName)) {
        $fileInfo = $uploadedIds[$recoveryFileName]
        Upload-RecoveryUpdaterLink -fileId $fileInfo.id -parentId $fileInfo.parentId -osName "linux-amd" -fullVersion $version
    }
    Write-Host " - Linux AMD64 Files - \n"
}

if ($os -eq "linux-arm") {
    Write-Host " - Linux ARM64 Files - " # Linux ARM
    $linux_arm_files = @(
        @("$app-arm64.AppImage", $true),
        @("$app-arm64.AppImage.sha256", $true),
        @("kDrive.dbg", $true),
        @("kDrive_client.dbg", $true),
        @("kDrive.src.zip", $true),
        @("kDrive_client.src.zip", $true),
        @("kDriveRecoveryUpdater-$version-arm64.AppImage", $true),
        @("kDriveRecoveryUpdater-$version-arm64.AppImage.sha256", $true)
    )
    $uploadedIds = Upload-FilesToKDrive -directory build-linux-arm64 -files $linux_arm_files -targetSubDir "linux-arm"

    $recoveryFileName = "kDriveRecoveryUpdater-$version-arm64.AppImage"
    if ($uploadedIds.ContainsKey($recoveryFileName)) {
        $fileInfo = $uploadedIds[$recoveryFileName]
        Upload-RecoveryUpdaterLink -fileId $fileInfo.id -parentId $fileInfo.parentId -osName "linux-arm" -fullVersion $version
    }
    Write-Host " - Linux ARM64 Files - \n"
}
