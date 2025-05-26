# infomaniak-build-tools/conan/find_conan_dep.ps1

param(
    [Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$Version
)

function Get-ConanExePath {
    try {
        $cmd = Get-Command conan.exe -ErrorAction Stop
        Log "Conan executable found at: $($cmd.Path)"
        return $cmd.Path
    } catch { }

    try {
        $pythonCmd = Get-Command python -ErrorAction Stop

        $venvCheck = & $pythonCmd -c 'import sys; print(sys.prefix != sys.base_prefix)'
        if($venvCheck.Trim().ToLower() -ne "true") {
            Write-Warning "Python virtual environment not activated. It is recommended to install conan with a virtual env."
        }
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

    Log "Conan executable found at: $exePath"
    return $exePath
}

$reference = "$Package/$Version"

# Vérification de sqlite3
if (-not (Get-Command sqlite3 -ErrorAction SilentlyContinue)) {
    Write-Error "Veuillez installer sqlite3 pour utiliser ce script."
    exit 1
}

$conan_exe = Get-ConanExePath
$conan_home = & $conan_exe config home
$conan_db = Join-Path $conan_home "p\cache.sqlite3"

if (-not (Test-Path $conan_db)) {
    Write-Error "Conan database not found at $conan_db. Please execute this script from the root of the project: ./infomaniak-build-tools/conan/build_dependencies.ps1"
    exit 1
}

$sql_req = "SELECT path FROM packages WHERE reference='$reference' ORDER BY timestamp DESC LIMIT 1; .exit"

$subpath = & sqlite3 $conan_db $sql_req 2>$null

if (-not $subpath) {
    Write-Error "Paquet $reference non trouvé dans le cache Conan."
    exit 1
}

Write-Output (Join-Path $conan_home "p\$subpath\p")