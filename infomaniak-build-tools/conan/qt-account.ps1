#!/usr/bin/env pwsh
#
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

# Qt account provisioning for Windows dependency builds.
# Dot-source after defining Log; loading this file only defines functions.

function Test-QtAccountContents([string]$Contents)
{
    $Contents = $Contents.TrimStart([char]0xFEFF)
    $inAccountSection = $false
    $values = @{}
    foreach ($line in ($Contents -split "\r\n|\n|\r"))
    {
        $line = $line.Trim()
        if ($line -match '^\[(.+)\]$')
        {
            $inAccountSection = $Matches[1] -eq "QtAccount"
        }
        elseif ($inAccountSection -and $line -match '^(email|jwt)\s*=(.*)$')
        {
            $values[$Matches[1]] = $Matches[2].Trim().Trim('"')
        }
    }
    # Check the file structure only. The installer validates the credentials with Qt.
    return -not [string]::IsNullOrWhiteSpace($values["email"]) -and
           -not [string]::IsNullOrWhiteSpace($values["jwt"])
}

function Test-QtAccountFile([string]$Path)
{
    $bytes = [IO.File]::ReadAllBytes($Path)
    try
    {
        # Qt 6 reads INI files as UTF-8; do not auto-detect UTF-16 like ReadAllText does.
        $contents = [Text.UTF8Encoding]::new($false, $true).GetString($bytes)
    }
    catch [Text.DecoderFallbackException]
    {
        return $false
    }
    return Test-QtAccountContents $contents
}

function Initialize-QtAccount
{
    $appData = $env:APPDATA
    if ([string]::IsNullOrWhiteSpace($appData))
    {
        $appData = Join-Path ([Environment]::GetFolderPath("UserProfile")) "AppData\Roaming"
    }
    $accountDirectory = Join-Path $appData "Qt"
    $accountFile = Join-Path $accountDirectory "qtaccount.ini"
    Log "Qt account file: $accountFile"
    $accountExists = Test-Path -LiteralPath $accountFile -PathType Leaf
    if ($accountExists)
    {
        if (Test-QtAccountFile $accountFile)
        {
            Log "Using the existing Qt account file with email and JWT."
            return
        }
        Log "The existing Qt account file is not UTF-8 or has no usable QtAccount email/JWT pair."
    }
    if ([string]::IsNullOrWhiteSpace($env:QT_ACCOUNT_INI))
    {
        throw "Qt account file is missing or unusable. Configure the GitHub secret QT_ACCOUNT_INI with the complete qtaccount.ini contents."
    }
    if (-not (Test-QtAccountContents $env:QT_ACCOUNT_INI))
    {
        throw "QT_ACCOUNT_INI must contain complete INI text with a [QtAccount] section and non-empty email and jwt values. Supply actual newlines, not base64 or literal \n sequences."
    }

    New-Item -ItemType Directory -Path $accountDirectory -Force | Out-Null
    $temporaryFile = Join-Path $accountDirectory ("qtaccount-" + [guid]::NewGuid().ToString("N") + ".tmp")
    try
    {
        [IO.File]::WriteAllText($temporaryFile, $env:QT_ACCOUNT_INI, [Text.UTF8Encoding]::new($false))
        if ($accountExists)
        {
            # Preserve credentials another process may have refreshed since the initial check.
            if (Test-QtAccountFile $accountFile)
            {
                Log "Using the existing Qt account file with email and JWT."
                return
            }
            [IO.File]::Replace($temporaryFile, $accountFile, [NullString]::Value)
        }
        else
        {
            # Move fails if another process created the destination in the meantime.
            [IO.File]::Move($temporaryFile, $accountFile)
        }
    }
    finally
    {
        if (Test-Path -LiteralPath $temporaryFile) { Remove-Item -LiteralPath $temporaryFile -Force }
    }
    Log "Prepared the Qt account file from QT_ACCOUNT_INI."
}
