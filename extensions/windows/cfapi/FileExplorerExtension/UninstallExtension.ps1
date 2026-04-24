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

# Requires administrative privileges to remove AppX packages
$packageNamePattern = "Infomaniak.kDrive.Extension*"

Write-Output "Searching for provisioned packages containing 'Infomaniak.kDrive.Extension'..." 
$provisionedPackage = Get-AppxProvisionedPackage -Online | Where-Object {$_.PackageName -like $packageNamePattern}

if (-not $provisionedPackage) {
    Write-Output "No provisioned package found matching: $packageNamePattern" 
}
else {
    Write-Output "Found the following package(s):" 
    $provisionedPackage | Format-List PackageName, InstallLocation

    foreach ($pkg in $provisionedPackage) {
        try {
            Write-Output "Removing package: $($pkg.PackageName)" 
            Remove-AppxProvisionedPackage -Online  -AllUsers -PackageName $pkg.PackageName -ErrorAction Stop
            Remove-AppxPackage -AllUsers -Package $pkg.PackageName -ErrorAction Stop
            Write-Output "Successfully removed: $($pkg.PackageName)"
        }
        catch {
            Write-Output "Failed to remove package '$($pkg.PackageName)': $_"
    		exit 1
        }
    }
}

Write-Output "Searching for local packages containing 'Infomaniak.kDrive.Extension'..."
$package = Get-AppxPackage -AllUsers -Name $packageNamePattern

if (-not $package) {
    Write-Output "No package found matching: $packageNamePattern"
    exit 0
}

Write-Output "Found the following package(s):"
$package | Format-List Name, PackageFullName, InstallLocation

foreach ($pkg in $package) {
    try {
        Write-Output "Removing package: $($pkg.PackageFullName)"
        Remove-AppxPackage -AllUsers -Package $pkg.PackageFullName -ErrorAction SilentlyContinue
        Write-Output "Successfully removed: $($pkg.PackageFullName)" 
    }
    catch {
        Write-Output "Failed to remove package '$($pkg.PackageFullName)': $_"
		exit 1
    }
}

exit 0