# We should clean up parameters files before uninstalling the extension because removing the LiteSync leads to deletion of all the dehydrated placeholders.
# In the case the uninstallation of the LiteSync failed but the dehydrated placeholders have been deleted, we do not want to have valid synchronization set up because it will propagate all the DELETE opearations on the server.

# Clean kDrive folders for all users
$profiles = Get-ChildItem "C:\Users" -Directory -ErrorAction SilentlyContinue

foreach ($profile in $profiles) {

    $localAppData = Join-Path $profile.FullName "AppData\Local"
    $tempFolder   = Join-Path $localAppData "Temp"

    $pathsToDelete = @(
        Join-Path $localAppData "kDrive"
        Join-Path $tempFolder   "kDrive-logdir"
        Join-Path $tempFolder   "kDrive-cache"
    )

    foreach ($path in $pathsToDelete) {
        if (Test-Path $path) {
            Write-Host "Deleting $path"
            try {
                Remove-Item $path -Recurse -Force -ErrorAction SilentlyContinue
                Write-Host " → Deleted" -ForegroundColor Green
            }
            catch {
                Write-Warning " → Failed to delete $path : $_"
            }
        }
    }
}

# Remove shortcuts
## Start menu shortcut
Remove-Item "%ProgramData%\Microsoft\Windows\Start Menu\Programs\kDrive.lnk"
Remove-Item "%AppData%\Microsoft\Windows\Start Menu\Programs\kDrive.lnk"
## Desktop shortcut
$DesktopPath = [Environment]::GetFolderPath("Desktop")
Remove-Item "$DesktopPath\kDrive.lnk" -Force -ErrorAction SilentlyContinue
## Quick Launch shortcut
Remove-Item $quickLaunchPath\${APPLICATION_NAME}.lnk -Force -ErrorAction SilentlyContinue

# Remove credentials
$CredentialsToDelete = 'com.infomaniak.drive.desktopclient*'
$Credentials = cmdkey.exe /list:($CredentialsToDelete) | Select-String -Pattern 'Target:*'
$Credentials = $Credentials -replace ' ', '' -replace 'Target:', ''
foreach ($Credential in $Credentials) {cmdkey.exe /delete $Credential | Out-Null}

# Remove SyncRoot registration
$syncRootRegPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager"

Write-Host "Scanning SyncRootManager registry entries..."

Get-ChildItem $syncRootRegPath -ErrorAction SilentlyContinue | ForEach-Object {
    $key = $_
    $props = Get-ItemProperty -Path $key.PSPath -ErrorAction SilentlyContinue
    $defaultValue = $props.'(default)'

    if ($defaultValue -eq "kDrive") {
        $syncRootId = $key.PSChildName
        Write-Host "Deleting SyncRoot key: $syncRootId"

        try {
            Remove-Item -Path $key.PSPath -Recurse -Force -ErrorAction SilentlyContinue
            Write-Host " → Deleted successfully" -ForegroundColor Green
        }
        catch {
            Write-Warning " → Failed to delete $syncRootId : $_"
        }
    }
}

exit 0