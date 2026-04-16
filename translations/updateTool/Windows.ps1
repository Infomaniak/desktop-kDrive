# Default path to lupdate.exe
$lupdatePath = "C:\Qt\Tools\QtDesignStudio\qt6_design_studio_reduced_version\bin\lupdate.exe"

# Verify again the path provided by the user
while (-Not (Test-Path $lupdatePath)) {
    Write-Host "The provided path does not exist."
    $lupdatePath = Read-Host "Please provide the correct path to lupdate.exe"
}

# Find all .ts files in the translations folder
$tsFiles = Get-ChildItem -Path ../ -Filter "*.ts" -Recurse

# Path to the src folder
$srcPath = "..\..\src"

# Counter for updated files
$updatedCount = 0

# Run lupdate.exe for each .ts file
foreach ($file in $tsFiles) {
    & $lupdatePath $srcPath -ts $file.FullName -noobsolete
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error while running lupdate.exe for $($file.FullName)"
    } else {
        Write-Host "Update successful for $($file.FullName)"
        $updatedCount++
    }
}

# Display the number of updated files
Write-Host "Total number of files updated: $updatedCount"
