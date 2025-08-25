# Requires administrative privileges to remove AppX packages

# Step 1: Find the full package name(s) containing "Infomaniak.kDrive.Extension"
Write-Output "Searching for packages containing 'Infomaniak.kDrive.Extension'..." -ForegroundColor Yellow

$packageNamePattern = "Infomaniak.kDrive.Extension*"
$package = Get-AppxPackage -Name $packageNamePattern

if (-not $package) {
    Write-Output "No package found matching: $packageNamePattern" -ForegroundColor Red
    exit 0
}

# Step 2: Display found package(s)
Write-Output "Found the following package(s):" -ForegroundColor Green
$package | Format-List Name, PackageFullName, InstallLocation

# Step 3: Remove each matching package
foreach ($pkg in $package) {
    try {
        Write-Output "Removing package: $($pkg.PackageFullName)" -ForegroundColor Yellow
        Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction Stop
        Write-Output "Successfully removed: $($pkg.PackageFullName)" -ForegroundColor Green
    }
    catch {
        Write-Output "Failed to remove package '$($pkg.PackageFullName)': $_"
		exit 1
    }
}

exit 0