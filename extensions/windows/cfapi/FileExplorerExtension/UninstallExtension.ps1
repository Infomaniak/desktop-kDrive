# Requires administrative privileges to remove AppX packages

# Step 1: Find the full package name(s) containing "Infomaniak.kDrive.Extension"
Write-Output "Searching for packages containing 'Infomaniak.kDrive.Extension'..." 

$packageNamePattern = "Infomaniak.kDrive.Extension*"
$provisionedPackage = Get-AppxProvisionedPackage -Online | Where-Object {$_.PackageName -like $packageNamePattern}

if (-not $provisionedPackage) {
    Write-Output "No package found matching: $packageNamePattern" 
    exit 0
}

# Step 2: Display found package(s)
Write-Output "Found the following package(s):" 
$provisionedPackage | Format-List PackageName, InstallLocation

# Step 3: Remove each matching package
foreach ($pkg in $provisionedPackage) {
    try {
        Write-Output "Removing package: $($pkg.PackageName)" 
        Remove-AppxPackage -AllUsers -Package $pkg.PackageName -ErrorAction Stop
        Write-Output "Successfully removed: $($pkg.PackageName)"
    }
    catch {
        Write-Output "Failed to remove package '$($pkg.PackageName)': $_"
		exit 1
    }
}

exit 0