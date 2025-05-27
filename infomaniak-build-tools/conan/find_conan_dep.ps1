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
param(
    [Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$Version,
    [Parameter(Mandatory=$false)][switch]$CI = $false,
    [Parameter(Mandatory=$false)][switch]$VerboseLogging = $false,
    [Parameter(Mandatory=$false)][string]$PythonVenvPath = "C:\Program Files\Python313\.venv"
)

function Log { Write-Host "[INFO] $($args -join ' ')" }
function Err { Write-Error "[ERROR] $($args -join ' ')" ; exit 1 }
function Get-ConanExePath {
    try {
        $cmd = Get-Command conan.exe -ErrorAction Stop
        return $cmd.Path
    } catch { }

    try {
        $pythonCmd = Get-Command python -ErrorAction Stop
        if ($VerboseLogging) { Write-Error "python interpreter found at: $($pythonCmd.Path)" }
    } catch {
        Err "Interpreter 'python' not found. Please install Python 3 and/or add it to the PATH."
        return $null
    }

    $pythonCode = @"
import os, sys
try:
    import conans
except ImportError:
    sys.exit(1)
modpath = conans.__file__
prefix = modpath.split(os.sep + 'Lib' + os.sep)[0]
exe = os.path.join(prefix, 'Scripts', 'conan.exe')
print(exe)
"@

    $rawPath = & $pythonCmd.Path -c $pythonCode 2>$null
    $exePath = $rawPath.Trim()
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exePath)) {
        Err "Unable to locate 'conan.exe' via Python."
        return $null
    }
    if ($VerboseLogging) { Write-Error "Conan executable found at: $($cmd.Path)" }
    return $exePath
}

if ($CI) {
    & "$PythonVenvPath\Scripts\activate.ps1"
    if ($VerboseLogging) { Write-Error "CI Mode enabled." }
}

$reference = "$Package/$Version"

# Vérification de sqlite3
if (-not (Get-Command sqlite3 -ErrorAction SilentlyContinue)) {
    Err "Please install sqlite3 and add it to the PATH."
}

$conan_exe = Get-ConanExePath
if (-not $conan_exe) { Err "Conan executable not found." }
if ($CI) {
    $conan_home = "C:\Windows\System32\config\systemprofile\.conan2"
} else {
    $conan_home = & "$conan_exe" config home | Select-Object -First 1
    if ($VerboseLogging) { Write-Error "conan config home: $(conan_home)" }
}
$conan_db = Join-Path $conan_home "p\cache.sqlite3"

if (-not (Test-Path $conan_db)) {
    Err "Conan database not found at $conan_db. Please execute this script from the root of the project: ./infomaniak-build-tools/conan/build_dependencies.ps1"
}

$sql_req = "SELECT path FROM packages WHERE reference='$reference' ORDER BY timestamp DESC LIMIT 1;"

$subpath = & sqlite3 "$conan_db" "$sql_req" 2>$null

if (-not $subpath) {
    Err "The reference '$reference' does not exists in the conan cache.."
}

if (-not (Test-Path (Join-Path $conan_home "p\$subpath\p"))) {
    Err "The path to the folder found, does not exists. : $(Join-Path $conan_home "p\$subpath\p")"
} else {
    Write-Output (Join-Path $conan_home "p\$subpath\p")
}