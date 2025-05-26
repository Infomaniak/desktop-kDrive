# infomaniak-build-tools/conan/find_conan_dep.ps1

param(
    [Parameter(Mandatory=$true)][string]$Package,
    [Parameter(Mandatory=$true)][string]$Version
)

$reference = "$Package/$Version"

# Vérification de sqlite3
if (-not (Get-Command sqlite3 -ErrorAction SilentlyContinue)) {
    Write-Error "Veuillez installer sqlite3 pour utiliser ce script."
    exit 1
}

$conan_home = & conan config home
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