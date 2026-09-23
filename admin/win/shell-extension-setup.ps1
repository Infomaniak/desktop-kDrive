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

<#
.SYNOPSIS
    Deploys or removes the kDrive File Explorer extension (MSIX package).

.DESCRIPTION
    Helper embedded in the kDrive Windows installer. The installer extracts it into a
    temporary directory and runs it elevated: it is not meant to be started manually and
    is never deployed on the user machine.

    Install mode, run on a first installation as well as on an update:
      - removes the extension packages signed with another certificate (i.e. having another
        publisher id, hence another AUMID). Those belong to another package family, which Windows
        cannot update in place: on Windows 11 they would pile up next to the new one, and on
        Windows 10 they prevent it from being installed at all,
      - provisions the extension shipped with the installer, so that every user of the computer
        gets it.

    The AUMID of the kDrive sync roots is cleared before those packages are removed. This detaches
    the sync roots from the package being removed so that Windows does not tear them down with it,
    leaving the placeholders (i.e. the user files) untouched. kDrive points the sync roots at the
    newly deployed extension on its next start.

    Uninstall mode, only run on a full uninstallation:
      - removes every kDrive extension package, whatever the certificate it was signed with.

    The sync roots are left attached to their package here, so that Windows tears them down along
    with the extension and removes the placeholders.

.NOTES
    The registry is always accessed through the 64 bit view, so that the script behaves the same
    whether it is started by a 32 bit or a 64 bit host process.
#>

[CmdletBinding()]
param (
    # Install: deploy the extension shipped with the installer. Uninstall: remove every kDrive extension.
    [Parameter(Mandatory = $true)]
    [ValidateSet("Install", "Uninstall")]
    [string] $Mode,

    # Path of the .msixbundle to provision. Install mode only.
    [string] $BundlePath = "",

    # Identity name of the extension package.
    [string] $PackageName = "Infomaniak.kDrive.Extension",

    # Publisher id (AUMID suffix) of the extension shipped with the installer. Install mode only.
    [string] $PublisherId = "",

    # Value kDrive writes in the default value of its SyncRootManager entries.
    [string] $ProviderName = "kDrive"
)

$ErrorActionPreference = "Continue"

$syncRootManagerSubKey = "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager"
$packageFamilyPrefix = "$PackageName" + "_"

$script:hasError = $false

# -----------------------------------------------------------------------------
# Logging
# -----------------------------------------------------------------------------

# Logging goes to the host so that it never pollutes the value returned by a function. The
# installer redirects it to its own log.
function Write-Log {
    param ([string] $Message)

    Write-Host "[kDrive][shellext] $Message"
}

function Write-WarningLog {
    param ([string] $Message)

    Write-Host "[kDrive][shellext][WARNING] $Message"
}

function Write-ErrorLog {
    param ([string] $Message)

    $script:hasError = $true
    Write-Host "[kDrive][shellext][ERROR] $Message"
}

# -----------------------------------------------------------------------------
# Packages
# -----------------------------------------------------------------------------

# Extracts the publisher id from a package full name, i.e. the last token of
# <name>_<version>_<architecture>_<resource id>_<publisher id>.
function Get-PublisherIdFromFullName {
    param ([string] $PackageFullName)

    if ([string]::IsNullOrWhiteSpace($PackageFullName)) { return "" }

    $tokens = $PackageFullName -split "_"
    if ($tokens.Length -lt 2) { return "" }

    return $tokens[$tokens.Length - 1]
}

function Get-ExtensionPackages {
    try {
        return @(Get-AppxPackage -AllUsers -Name $PackageName -ErrorAction Stop)
    }
    catch {
        Write-ErrorLog "Unable to list the installed extension packages: $($_.Exception.Message)"
        return @()
    }
}

function Get-ExtensionProvisionedPackages {
    try {
        return @(Get-AppxProvisionedPackage -Online -ErrorAction Stop |
            Where-Object { $_.DisplayName -eq $PackageName })
    }
    catch {
        Write-ErrorLog "Unable to list the provisioned extension packages: $($_.Exception.Message)"
        return @()
    }
}

function Remove-ExtensionProvisionedPackage {
    param ([string] $PackageFullName)

    Write-Log "Removing provisioned package '$PackageFullName'."

    try {
        Remove-AppxProvisionedPackage -Online -PackageName $PackageFullName -AllUsers -ErrorAction Stop | Out-Null
        return $true
    }
    catch {
        # -AllUsers is not supported by every Windows build, retry without it.
        try {
            Remove-AppxProvisionedPackage -Online -PackageName $PackageFullName -ErrorAction Stop | Out-Null
            return $true
        }
        catch {
            Write-ErrorLog "Unable to remove provisioned package '$PackageFullName': $($_.Exception.Message)"
            return $false
        }
    }
}

function Remove-ExtensionPackage {
    param ([string] $PackageFullName)

    Write-Log "Removing package '$PackageFullName'."

    try {
        Remove-AppxPackage -Package $PackageFullName -AllUsers -ErrorAction Stop
        return $true
    }
    catch {
        # -AllUsers is not supported by every Windows build, retry without it.
        try {
            Remove-AppxPackage -Package $PackageFullName -ErrorAction Stop
            return $true
        }
        catch {
            Write-ErrorLog "Unable to remove package '$PackageFullName': $($_.Exception.Message)"
            return $false
        }
    }
}

# Removes the extension packages. When $KeepPublisherId is set, the packages signed with that
# publisher id are kept, only the ones coming from another certificate are removed.
function Remove-ExtensionPackages {
    param ([string] $KeepPublisherId = "")

    $removedCount = 0

    foreach ($provisionedPackage in (Get-ExtensionProvisionedPackages)) {
        $candidatePublisherId = Get-PublisherIdFromFullName $provisionedPackage.PackageName
        if ($KeepPublisherId -and ($candidatePublisherId -eq $KeepPublisherId)) { continue }

        if (Remove-ExtensionProvisionedPackage $provisionedPackage.PackageName) { $removedCount++ }
    }

    foreach ($package in (Get-ExtensionPackages)) {
        $candidatePublisherId = Get-PublisherIdFromFullName $package.PackageFullName
        if ($KeepPublisherId -and ($candidatePublisherId -eq $KeepPublisherId)) { continue }

        if (Remove-ExtensionPackage $package.PackageFullName) { $removedCount++ }
    }

    return $removedCount
}

# Lists the publisher ids of the extension packages that are not signed with $PublisherId.
function Get-ForeignPublisherIds {
    $foreignPublisherIds = New-Object System.Collections.Generic.List[string]

    $fullNames = @()
    $fullNames += (Get-ExtensionProvisionedPackages | ForEach-Object { $_.PackageName })
    $fullNames += (Get-ExtensionPackages | ForEach-Object { $_.PackageFullName })

    foreach ($fullName in $fullNames) {
        # Note: variable names are case insensitive in PowerShell, this one must not collide with
        # the $PublisherId parameter.
        $candidatePublisherId = Get-PublisherIdFromFullName $fullName
        if (-not $candidatePublisherId -or ($candidatePublisherId -eq $PublisherId)) { continue }
        if ($foreignPublisherIds.Contains($candidatePublisherId)) { continue }

        Write-Log "Extension package signed with another certificate found: '$fullName'."
        $foreignPublisherIds.Add($candidatePublisherId) | Out-Null
    }

    return $foreignPublisherIds.ToArray()
}

# -----------------------------------------------------------------------------
# Sync roots
# -----------------------------------------------------------------------------

function Open-SyncRootManagerKey {
    try {
        $localMachine = [Microsoft.Win32.RegistryKey]::OpenBaseKey(
            [Microsoft.Win32.RegistryHive]::LocalMachine,
            [Microsoft.Win32.RegistryView]::Registry64)

        return $localMachine.OpenSubKey($syncRootManagerSubKey, $false)
    }
    catch {
        Write-ErrorLog "Unable to open the SyncRootManager registry key: $($_.Exception.Message)"
        return $null
    }
}

# Clears the AUMID of every kDrive sync root, which detaches them from the extension package.
# This is required before removing a package that has to be kept usable afterwards, otherwise
# Windows tears the sync roots down along with it and the placeholders are lost.
function Clear-SyncRootsAumid {
    $syncRootManagerKey = Open-SyncRootManagerKey
    if (-not $syncRootManagerKey) { return }

    $updatedCount = 0

    try {
        foreach ($syncRootName in $syncRootManagerKey.GetSubKeyNames()) {
            $syncRootKey = $null

            try {
                $syncRootKey = $syncRootManagerKey.OpenSubKey($syncRootName, $true)
                if (-not $syncRootKey) { continue }

                $provider = [string] $syncRootKey.GetValue("")
                $currentAumid = [string] $syncRootKey.GetValue("AUMID")

                # A sync root belongs to kDrive when it was registered by the application itself or
                # when it still points to one of the kDrive extension packages.
                if (($provider -ne $ProviderName) -and ($currentAumid -notlike "$packageFamilyPrefix*")) { continue }
                if ([string]::IsNullOrEmpty($currentAumid)) { continue }

                $syncRootKey.SetValue("AUMID", "", [Microsoft.Win32.RegistryValueKind]::String)
                $updatedCount++

                Write-Log "Sync root '$syncRootName' detached from '$currentAumid'."
            }
            catch {
                Write-ErrorLog "Sync root '$syncRootName': unable to clear the AUMID: $($_.Exception.Message)"
            }
            finally {
                if ($syncRootKey) { $syncRootKey.Close() }
            }
        }
    }
    finally {
        $syncRootManagerKey.Close()
    }

    Write-Log "$updatedCount sync root(s) detached."
}

# -----------------------------------------------------------------------------
# Install
# -----------------------------------------------------------------------------

function Install-Extension {
    if (-not (Test-Path -LiteralPath $BundlePath)) {
        Write-ErrorLog "The extension package '$BundlePath' does not exist."
        return
    }

    if ([string]::IsNullOrWhiteSpace($PublisherId)) {
        Write-WarningLog "No publisher id provided, the extensions signed with another certificate are kept."
    }
    else {
        $foreignPublisherIds = @(Get-ForeignPublisherIds)

        if ($foreignPublisherIds.Count -eq 0) {
            Write-Log "No extension signed with another certificate found."
        }
        else {
            Write-Log "$($foreignPublisherIds.Count) publisher id(s) to remove: $($foreignPublisherIds -join ', ')."

            # Detach the sync roots before removing the packages they point to, otherwise Windows
            # unregisters the sync roots and the placeholders they contain along with the package.
            Clear-SyncRootsAumid

            $removedCount = Remove-ExtensionPackages -KeepPublisherId $PublisherId
            Write-Log "$removedCount extension package(s) removed."
        }
    }

    # Provision the extension: every user of the computer gets it, current users at their next
    # logon, new users when their profile is created.
    try {
        Add-AppxProvisionedPackage -Online -PackagePath $BundlePath -SkipLicense -ErrorAction Stop | Out-Null

        Write-Log "Extension provisioned for every user of the computer."
    }
    catch {
        Write-WarningLog "Unable to provision the extension: $($_.Exception.Message)"
        Write-WarningLog "Falling back to an installation for the current user only."

        try {
            Add-AppxPackage -Path $BundlePath -ForceApplicationShutdown -ForceUpdateFromAnyVersion -ErrorAction Stop

            Write-Log "Extension installed for the current user."
        }
        catch {
            Write-ErrorLog "Unable to install the extension: $($_.Exception.Message)"
        }
    }

    # The sync roots are left detached: kDrive points them at the extension it expects on its next
    # start, in CloudProviderRegistrar::registerWithShell().
}

# -----------------------------------------------------------------------------
# Uninstall
# -----------------------------------------------------------------------------

function Uninstall-Extension {
    # Unlike the install mode, the sync roots are left attached to their package on purpose: this
    # is a full uninstallation, Windows has to tear them down along with the extension so that the
    # placeholders are removed too.
    $removedCount = Remove-ExtensionPackages
    Write-Log "$removedCount extension package(s) removed."
}

# -----------------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------------

Write-Log "Mode: $Mode. Package: $PackageName."

if ($Mode -eq "Install") {
    Install-Extension
}
else {
    Uninstall-Extension
}

if ($script:hasError) {
    Write-Log "Completed with errors."
    exit 1
}

Write-Log "Completed."
exit 0
