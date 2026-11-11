# Base directory = folder where the script is located
$scriptRoot = $PSScriptRoot

# Default path to lupdate.exe (relative to script)
$lupdatePath = Join-Path $scriptRoot "linguist\lupdate.exe"

# Verify again the path provided by the user
while (-Not (Test-Path $lupdatePath)) {
    Write-Host "The provided path does not exist."
    $lupdatePath = Read-Host "Please provide the correct path to lupdate.exe"
}

# Path to src folder (relative to script)
$srcPath = Join-Path $scriptRoot "..\..\src"

# Sources scanned by lupdate: src without gui4, whose frontends own their catalogs (Loco for Linux v4).
$srcPaths = Get-ChildItem -Path $srcPath -Directory | Where-Object { $_.Name -ne "gui4" } | ForEach-Object { $_.FullName }

# Find all .ts files in the translations folder (relative to script)
$translationsPath = Join-Path $scriptRoot ".."
$tsFiles = Get-ChildItem -Path $translationsPath -Filter "*.ts" -Recurse

# Counter for updated files
$updatedCount = 0

# Run lupdate.exe for each .ts file
foreach ($file in $tsFiles) {
    & $lupdatePath @srcPaths -ts $file.FullName -noobsolete

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error while running lupdate.exe for $($file.FullName) ($LASTEXITCODE)"
    } else {
        Write-Host "Update successful for $($file.FullName)"
        $updatedCount++
    }
}

# Display the number of updated files
Write-Host "Total number of files updated: $updatedCount"