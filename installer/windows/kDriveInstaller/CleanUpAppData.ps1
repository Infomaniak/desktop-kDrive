# We should clean up parameters files before uninstalling the extension because removing the LiteSync leads to deletion of all the dehydrated placeholders.
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

exit 0