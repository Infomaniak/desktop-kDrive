# Requires administrative privileges to remove AppX packages

# Step 1: Find the full package name(s) containing "Infomaniak.kDrive.Extension"
Write-Host "Searching for packages containing 'Infomaniak.kDrive.Extension'..." -ForegroundColor Yellow

$packageNamePattern = "Infomaniak.kDrive.Extension*"
$package = Get-AppxPackage -Name $packageNamePattern

if (-not $package) {
    Write-Host "No package found matching: $packageNamePattern" -ForegroundColor Red
    exit 1
}

# Step 2: Display found package(s)
Write-Host "Found the following package(s):" -ForegroundColor Green
$package | Format-List Name, PackageFullName, InstallLocation

# Step 3: Remove each matching package
foreach ($pkg in $package) {
    try {
        Write-Host "Removing package: $($pkg.PackageFullName)" -ForegroundColor Yellow
        Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction Stop
        Write-Host "Successfully removed: $($pkg.PackageFullName)" -ForegroundColor Green
    }
    catch {
        Write-Error "Failed to remove package '$($pkg.PackageFullName)': $_"
		exit 1
    }
}

exit 1