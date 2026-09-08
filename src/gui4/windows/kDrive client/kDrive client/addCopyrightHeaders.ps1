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

[CmdletBinding()]
param(
    [string]$RootDirectory = (Get-Location).Path
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $RootDirectory -PathType Container)) {
    throw "Directory '$RootDirectory' does not exist."
}

$copyrightMarker = "Copyright (C) 2023-2026 Infomaniak Network SA"

$csHeader = @"
/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

"@

$xamlHeader = @"
<!--
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2026 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 -->

"@

$files = Get-ChildItem -LiteralPath $RootDirectory -Recurse -File | Where-Object {
    ($_.Extension -in ".cs", ".xaml") -and
    ($_.FullName -notmatch "[\\/](bin|obj|\.git|packages)[\\/]")
}

$totalFiles = $files.Count
$updatedCount = 0
$skippedCount = 0
$processedCount = 0
$progressId = 1

if ($totalFiles -eq 0) {
    Write-Host "No .cs or .xaml files found under '$RootDirectory'."
    return
}

foreach ($file in $files) {
    $processedCount++
    $percentComplete = [int](($processedCount * 100) / $totalFiles)

    Write-Progress -Id $progressId -Activity "Adding copyright headers" -Status "$processedCount/$totalFiles ($percentComplete%) - $($file.Name)" -PercentComplete $percentComplete

    $content = Get-Content -LiteralPath $file.FullName -Raw

    if ($content -match [Regex]::Escape($copyrightMarker)) {
        $skippedCount++
        continue
    }

    if ($file.Extension -eq ".cs") {
        $newContent = $csHeader + $content
    }
    else {
        if ($content -match "^(\s*<\?xml[^>]*\?>)(\r?\n)?") {
            $xmlDeclaration = $matches[1]
            $remainingContent = $content.Substring($xmlDeclaration.Length).TrimStart("`r", "`n")
            $newContent = "$xmlDeclaration`r`n$xamlHeader$remainingContent"
        }
        else {
            $newContent = $xamlHeader + $content
        }
    }

    [System.IO.File]::WriteAllText($file.FullName, $newContent, [System.Text.Encoding]::UTF8)
    $updatedCount++
}

Write-Progress -Id $progressId -Activity "Adding copyright headers" -Completed
Write-Host "Updated $updatedCount file(s). Skipped $skippedCount file(s) that already contain a copyright header."