# We should clean up parameters files before uninstalling the extension because removing the LiteSync leads to deletion of all the dehydrated placeholders.
# In the case the uninstallation of the LiteSync failed but the dehydrated placeholders have been deleted, we do not want to have valid synchronization set up because it will propagate all the DELETE operations on the server.

# Clean kDrive folders for all users
$profilesFolder = [Environment]::GetFolderPath("UserProfile") + "\..\"
$profiles = Get-ChildItem $profilesFolder -Directory -ErrorAction Continue

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
            Remove-Item $path -Recurse -Force -ErrorAction Continue
        }
    }
}

# Remove shortcuts
## Start menu shortcut
Write-Host "Removing shortcuts..."
$DesktopPath = [Environment]::GetFolderPath("Desktop")
$pathsToDelete = @(
    "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\kDrive.lnk",
    "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\kDrive.lnk",
    "$DesktopPath\kDrive.lnk"
)

foreach ($path in $pathsToDelete) {
    if (Test-Path $path) {
        Write-Host "Deleting $path"
        Remove-Item $path -Force -ErrorAction Continue
    }
}

# Remove credentials
$CredentialsToDelete = 'com.infomaniak.drive.desktopclient*'
$Credentials = cmdkey.exe /list:($CredentialsToDelete) | Select-String -Pattern 'Target:*'
$Credentials = $Credentials -replace ' ', '' -replace 'Target:', ''
foreach ($Credential in $Credentials) {cmdkey.exe /delete $Credential | Out-Null}

# Remove SyncRoot registration
$syncRootRegPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager"

Write-Host "Scanning SyncRootManager registry entries..."

Get-ChildItem $syncRootRegPath -ErrorAction Continue | ForEach-Object {
    $key = $_
    $props = Get-ItemProperty -Path $key.PSPath -ErrorAction Continue
    $defaultValue = $props.'(default)'

    if ($defaultValue -eq "kDrive") {
        $syncRootId = $key.PSChildName
        Write-Host "Deleting SyncRoot key: $syncRootId"
        Remove-Item -Path $key.PSPath -Recurse -Force -ErrorAction Continue
    }
}

exit 0