<#
 Infomaniak kDrive - Desktop App
 Copyright (C) 2023-2026 Infomaniak Network SA

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
#>

# =============================================================================
# kDrive Shell Extension Repair Tool
# =============================================================================

$ErrorActionPreference = "Stop"

# -----------------------------------------------------------------------------
# Request administrator privileges
# -----------------------------------------------------------------------------

$currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$currentPrincipal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
$isAdministrator = $currentPrincipal.IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)

if (-not $isAdministrator) {

    Write-Host ""
    Write-Host "Administrator privileges are required." -ForegroundColor Yellow
    Write-Host "A Windows confirmation window will now be displayed." -ForegroundColor Gray
    Write-Host ""

    try {
        $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""

        Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList $arguments `
            -Verb RunAs

        exit
    }
    catch {
        Write-Host ""
        Write-Host "[ERROR] Administrator privileges were not granted." -ForegroundColor Red
        Write-Host "The repair cannot continue." -ForegroundColor Red
        Write-Host ""
        Read-Host "Press Enter to close"
        exit 1
    }
}

# -----------------------------------------------------------------------------
# Configuration
# -----------------------------------------------------------------------------

$syncRootManagerPath = "HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager"
$appxPath = $PSScriptRoot
$installPath = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$kDriveExePath = Join-Path $installPath "kDrive.exe"

$kDriveSyncRoots = 0
$ignoredSyncRoots = 0
$removedPackages = 0
$installedBundles = 0
$hasErrors = $false

Clear-Host

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "              kDrive Shell Extension Repair" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "This tool will repair the kDrive Windows shell extension." -ForegroundColor White
Write-Host "Please do not close this window while the repair is running." -ForegroundColor Gray
Write-Host ""

# -----------------------------------------------------------------------------
# 0. Stop running kDrive and shell extension instances
# -----------------------------------------------------------------------------

Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[0/5] Stopping kDrive and shell extension processes" -ForegroundColor Cyan
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

$processNames = @(
    "kDrive",
    "FileExplorerExtension"
)

foreach ($processName in $processNames) {

    $processes = Get-Process -Name $processName -ErrorAction SilentlyContinue

    if ($processes) {

        foreach ($process in $processes) {

            Write-Host "Stopping process:" -ForegroundColor White
            Write-Host "  Name: $($process.ProcessName)" -ForegroundColor Gray
            Write-Host "  PID:  $($process.Id)" -ForegroundColor Gray

            try {
                Stop-Process `
                    -Id $process.Id `
                    -Force `
                    -ErrorAction Stop

                Write-Host "  [OK] Process stopped." -ForegroundColor Green
            }
            catch {
                $hasErrors = $true

                Write-Host "  [ERROR] Failed to stop process." -ForegroundColor Red
                Write-Host "          $($_.Exception.Message)" -ForegroundColor Red
            }

            Write-Host ""
        }
    }
    else {
        Write-Host "[INFO] No running $processName instance found." -ForegroundColor DarkGray
    }
}

# -----------------------------------------------------------------------------
# 1. Clear AUMID for every kDrive SyncRootManager entry
# -----------------------------------------------------------------------------

Write-Host ""
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[1/5] Checking Windows SyncRoot registrations" -ForegroundColor Cyan
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

Write-Host "Searching SyncRootManager entries..." -ForegroundColor Gray

if (Test-Path $syncRootManagerPath) {

    Get-ChildItem $syncRootManagerPath | ForEach-Object {

        $syncRootKey = $_
        $defaultValue = $syncRootKey.GetValue("")

        Write-Host ""
        Write-Host "SyncRoot found:" -ForegroundColor White
        Write-Host "  Key:           $($syncRootKey.Name)" -ForegroundColor Gray
        Write-Host "  Default value: '$defaultValue'" -ForegroundColor Gray

        if ($defaultValue -eq "kDrive") {

            $kDriveSyncRoots++

            Write-Host "  [KDRIVE] kDrive SyncRoot detected" -ForegroundColor Green

            $currentAumid = $syncRootKey.GetValue("AUMID")

            Write-Host "  Current AUMID: '$currentAumid'" -ForegroundColor Gray
            Write-Host "  Clearing AUMID..." -ForegroundColor Yellow

            Set-ItemProperty -Path $syncRootKey.PSPath -Name "AUMID" -Value ""

            Write-Host "  [OK] AUMID cleared." -ForegroundColor Green
        }
        else {

            $ignoredSyncRoots++

            Write-Host "  [IGNORED] Not a kDrive SyncRoot." -ForegroundColor DarkGray
            Write-Host "            No modification performed." -ForegroundColor DarkGray
        }
    }
}
else {
    Write-Warning "SyncRootManager registry key not found."
    $hasErrors = $true
}

Write-Host ""
Write-Host "SyncRoot scan completed." -ForegroundColor Green
Write-Host "  kDrive entries:  $kDriveSyncRoots" -ForegroundColor Gray
Write-Host "  Ignored entries: $ignoredSyncRoots" -ForegroundColor Gray

# -----------------------------------------------------------------------------
# 2. Remove every installed kDrive extension
# -----------------------------------------------------------------------------

Write-Host ""
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[2/5] Removing existing kDrive shell extensions" -ForegroundColor Cyan
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

Write-Host "Searching for installed kDrive extensions..." -ForegroundColor Gray

$packages = Get-AppxPackage -AllUsers |
    Where-Object {
        $_.PackageFullName -like "Infomaniak.kDrive.Extension_*"
    }

if (-not $packages) {
    Write-Host "[INFO] No existing kDrive extension package found." -ForegroundColor DarkGray
}

foreach ($package in $packages) {

    Write-Host ""
    Write-Host "Removing:" -ForegroundColor White
    Write-Host "  $($package.PackageFullName)" -ForegroundColor Gray

    try {
        Remove-AppxPackage `
            -Package $package.PackageFullName `
            -AllUsers `
            -ErrorAction Stop

        $removedPackages++

        Write-Host "  [OK] Removed." -ForegroundColor Green
    }
    catch {
        $hasErrors = $true

        Write-Host "  [ERROR] Failed to remove package." -ForegroundColor Red
        Write-Host "          $($_.Exception.Message)" -ForegroundColor Red
    }
}

# -----------------------------------------------------------------------------
# 3. Install the kDrive shell extension
# -----------------------------------------------------------------------------

Write-Host ""
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[3/5] Installing the kDrive shell extension" -ForegroundColor Cyan
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

Write-Host "Searching for MSIX bundle in:" -ForegroundColor Gray
Write-Host "  $appxPath" -ForegroundColor Gray
Write-Host ""

if (-not (Test-Path $appxPath)) {

    Write-Host "[ERROR] The kDrive extension directory does not exist:" -ForegroundColor Red
    Write-Host "        $appxPath" -ForegroundColor Red

    $hasErrors = $true
}
else {

    $bundles = Get-ChildItem `
        -Path $appxPath `
        -Filter "*.msixbundle" `
        -File

    if (-not $bundles) {

        Write-Host "[ERROR] No .msixbundle was found in:" -ForegroundColor Red
        Write-Host "        $appxPath" -ForegroundColor Red

        $hasErrors = $true
    }
    else {

        foreach ($bundle in $bundles) {

            Write-Host "Installing:" -ForegroundColor White
            Write-Host "  $($bundle.FullName)" -ForegroundColor Gray

            try {
                Add-AppxPackage `
                    -Path $bundle.FullName `
                    -ForceApplicationShutdown `
                    -ErrorAction Stop

                $installedBundles++

                Write-Host "  [OK] Installed." -ForegroundColor Green
            }
            catch {
                $hasErrors = $true

                Write-Host "  [ERROR] Failed to install $($bundle.Name)." -ForegroundColor Red
                Write-Host "          $($_.Exception.Message)" -ForegroundColor Red
            }

            Write-Host ""
        }
    }
}

# -----------------------------------------------------------------------------
# 4. Restart Windows Explorer
# -----------------------------------------------------------------------------

Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[4/5] Restarting Windows Explorer" -ForegroundColor Cyan
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

try {

    $explorerProcesses = Get-Process -Name "explorer" -ErrorAction SilentlyContinue

    if ($explorerProcesses) {

        Write-Host "Stopping Windows Explorer..." -ForegroundColor Gray

        $explorerProcesses |
            Stop-Process -Force -ErrorAction Stop
    }

    Write-Host "Starting Windows Explorer..." -ForegroundColor Gray

    Start-Process "explorer.exe"

    Start-Sleep -Seconds 2

    Write-Host "[OK] Windows Explorer restarted." -ForegroundColor Green
}
catch {
    $hasErrors = $true

    Write-Host "[ERROR] Failed to restart Windows Explorer." -ForegroundColor Red
    Write-Host "        $($_.Exception.Message)" -ForegroundColor Red
}

# -----------------------------------------------------------------------------
# 5. Restart kDrive as the normal user
# -----------------------------------------------------------------------------

Write-Host ""
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "[5/5] Restarting kDrive" -ForegroundColor Cyan
Write-Host "------------------------------------------------------------" -ForegroundColor DarkGray
Write-Host ""

if (-not (Test-Path $kDriveExePath)) {

    $hasErrors = $true

    Write-Host "[ERROR] kDrive executable was not found:" -ForegroundColor Red
    Write-Host "        $kDriveExePath" -ForegroundColor Red
}
else {

    try {
        Write-Host "Starting kDrive as the current Windows user..." -ForegroundColor Gray
        Write-Host "  $kDriveExePath" -ForegroundColor Gray

        # The repair tool is elevated. Starting kDrive directly from this
        # process would also start it elevated.
        #
        # Delegate the launch to Explorer so kDrive starts in the normal
        # interactive user context.
        Start-Process `
            -FilePath "explorer.exe" `
            -ArgumentList "`"$kDriveExePath`""

        Write-Host "[OK] kDrive restarted." -ForegroundColor Green
    }
    catch {
        $hasErrors = $true

        Write-Host "[ERROR] Failed to restart kDrive." -ForegroundColor Red
        Write-Host "        $($_.Exception.Message)" -ForegroundColor Red
    }
}

# -----------------------------------------------------------------------------
# Summary
# -----------------------------------------------------------------------------

Write-Host ""
Write-Host "============================================================" -ForegroundColor Cyan

if ($hasErrors) {
    Write-Host "              Repair completed with errors" -ForegroundColor Yellow
}
else {
    Write-Host "              Repair completed successfully" -ForegroundColor Green
}

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host ""

Write-Host "Summary:" -ForegroundColor White
Write-Host "  kDrive SyncRoots repaired : $kDriveSyncRoots" -ForegroundColor Gray
Write-Host "  Other SyncRoots ignored   : $ignoredSyncRoots" -ForegroundColor Gray
Write-Host "  Extensions removed        : $removedPackages" -ForegroundColor Gray
Write-Host "  Extensions installed      : $installedBundles" -ForegroundColor Gray
Write-Host ""

if ($hasErrors) {
    Write-Host "Some operations could not be completed." -ForegroundColor Yellow
    Write-Host "Please provide the content of this window to kDrive support." -ForegroundColor Yellow
}
else {
    Write-Host "The kDrive shell extension has been repaired." -ForegroundColor Green
    Write-Host "kDrive has been restarted." -ForegroundColor Green
    Write-Host "You can now close this window." -ForegroundColor Gray
}

Write-Host ""
Read-Host "Press Enter to close"