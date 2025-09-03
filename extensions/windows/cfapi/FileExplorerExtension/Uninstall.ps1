#################################################################################################
#                                                                                               #
#                                            Clean up                                           #
#                                                                                               #
#################################################################################################

# We are cleaning up parameters files before uninstalling the extension because removing the LiteSync leads to deletion of all the dehydrated placeholders.
# In the case the uninstallation of the LiteSync failed but the dehydrated placeholders have been deleted, we do not want to have valid synchronization set up because it will propagate all the DELETE opearations on the server.

# Remove DB folders
Remove-Item "$env:LOCALAPPDATA\kDrive" -Recurse

# Remove log folder
Remove-Item "$env:TEMP\kDrive-logdir" -Recurse

# Remove cache folder
Remove-Item "$env:TEMP\kDrive-cache" -Recurse

# Remove shortcuts
## Start menu shortcut
Remove-Item "%ProgramData%\Microsoft\Windows\Start Menu\Programs\kDrive.lnk"
Remove-Item "%AppData%\Microsoft\Windows\Start Menu\Programs\kDrive.lnk"
## Desktop shortcut
$DesktopPath = [Environment]::GetFolderPath("Desktop")
Remove-Item "$DesktopPath\kDrive.lnk"
## Quick Launch shortcut
Remove-Item $quickLaunchPath\${APPLICATION_NAME}.lnk

# Remove credentials
$CredentialsToDelete = 'com.infomaniak.drive.desktopclient*'
$Credentials = cmdkey.exe /list:($CredentialsToDelete) | Select-String -Pattern 'Target:*'
$Credentials = $Credentials -replace ' ', '' -replace 'Target:', ''
foreach ($Credential in $Credentials) {cmdkey.exe /delete $Credential | Out-Null}

#################################################################################################
#                                                                                               #
#                                  Remove extentension package                                  #
#                                                                                               #
#################################################################################################

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