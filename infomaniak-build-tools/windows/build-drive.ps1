﻿<#
 Infomaniak kDrive - Desktop App
 Copyright (C) 2023-2025 Infomaniak Network SA

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

# Parameters :
Param(
    # BuildType	: The type of build (Debug will run the tests, Release will sign the app)
    [ValidateSet('Release', 'RelWithDebInfo', 'Debug')]
    [string] $buildType = "RelWithDebInfo",

    # Path	: The path to the root CMakeLists.txt
    [string] $path = $PWD.Path,

    # Clean	: The files to clean on execution
    [string] $clean,

    # Thumbprint: Specifies which certificate will be used to sign the app
    [string] $thumbprint,

    # Ext	: Rebuild the extension (automatically done if vfs.h is missing)
    [switch] $ext,

    # ci	: Build with CI testing (currently only checks the building stage)
    [switch] $ci,

    # Upload :	flag to trigger the use of the USB-key signing certificate
    [switch] $upload,

    # Help	: Displays the help message then exit if called
    [switch] $help
)

#################################################################################################
#                                                                                               #
#                                       PATHS AND VARIABLES                                     #
#                                                                                               #
#################################################################################################

# CMake will treat any backslash as escape character and return an error
$path = $path.Replace('\', '/')
$contentPath = "$path/build-windows"
$buildPath = "$contentPath/build"
$installPath = "$contentPath/install"

$extPath = "$path/extensions/windows/cfapi/"
$vfsDir = $extPath + "x64/Release"

# Files to be added to the archive and then packaged
$archivePath = "$installPath/bin"
$archiveName = "kDrive.7z"

# NSIS needs the path to use backslash
$archiveDataPath = ('{0}\build-windows\{1}' -f $path.Replace('/', '\'), $archiveName)

#################################################################################################
#                                                                                               #
#                                           FUNCTIONS                                           #
#                                                                                               #
#################################################################################################

function Clean {
    param (
        [string] $cleanPath
    )

    if (Test-Path -Path $cleanPath) {
        Remove-Item -Path $cleanPath -Recurse
    }
}

function Get-Thumbprint {
    param (
        [bool] $upload
    )

    $thumbprint = 
    If ($upload) {
        Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match "Infomaniak" -and $_.Issuer -match "EV" } | Select -ExpandProperty Thumbprint
    } 
    Else {
        Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match "Infomaniak" -and $_.Issuer -notmatch "EV" } | Select -ExpandProperty Thumbprint
    }
    return $thumbprint
}

function Get-Aumid {
    param (
        [bool] $upload
    )

    $aumid = if ($upload) { $env:KDC_PHYSICAL_AUMID } else { $env:KDC_VIRTUAL_AUMID }

    if (!$aumid) {
        Write-Host "The AUMID value could not be read from env.
                   Exiting." -f Red
        exit 1
    }

    return $aumid
}

function Get-App-Name {
    param (
        [string] $buildPath
    )

    $prodName = "kDrive"
    $version = (Select-String -Path $buildPath\version.h KDRIVE_VERSION_FULL | foreach-object { $data = $_ -split " "; echo $data[3] })
    $appName = "$prodName-$version.exe"

   
    return $appName
}

function Get-Installer-Path {
    param (
        [string] $buildPath,
        [string] $contentPath
    )

    $appName = Get-App-Name $buildPath
    $installerPath = "$contentPath/$appName"

    return $installerPath
}

function Build-Extension {
    param (
        [string] $path,
        [string] $extPath,
        [string] $buildType 
    )

    Write-Host "Building extension ($buildType) ..."

    $configuration = $buildType
    if ($buildType -eq "RelWithDebInfo") { $configuration = "Release" }

    msbuild "$extPath\kDriveExt.sln" /p:Configuration=$configuration /p:Platform=x64 /p:PublishDir="$extPath\FileExplorerExtensionPackage\AppPackages\" /p:DeployOnBuild=true

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $srcVfsPath = "$path\src\libcommonserver\vfs\win\."
    Copy-Item -Path "$extPath\Vfs\..\Common\debug.h" -Destination $srcVfsPath
    Copy-Item -Path "$extPath\Vfs\Vfs.h" -Destination $srcVfsPath

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "Extension built."
}

function CMake-Build-And-Install {
    param (
        [string] $path,
        [string] $installPath,
        [string] $vfsDir
    )
    Write-Host "1) Installing Conan dependencies…"
    & "$path\infomaniak-build-tools\conan\build_dependencies.ps1" -buildType $buildType -ci

    Write-Host "2) Configuring and building with CMake ..."

    $compiler = "cl.exe"

    $args = @("'-GNinja'")
    $args += ("'-DCMAKE_BUILD_TYPE=$buildType'")
    $args += ("'-DCMAKE_INSTALL_PREFIX=$installPath'")
    $args += ("'-DCMAKE_PREFIX_PATH=$installPath'")

    if ($ci) {
        $args += ("'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'")
    }

    $buildVersion = Get-Date -Format "yyyyMMdd"

    $flags = @(
        "'-DCMAKE_EXPORT_COMPILE_COMMANDS=1'",
        "'-DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe'",
        "'-DQT_QMAKE_EXECUTABLE:STRING=C:\Qt\Tools\CMake_64\bin\cmake.exe'",
        "'-DCMAKE_C_COMPILER:STRING=$compiler'",
        "'-DCMAKE_CXX_COMPILER:STRING=$compiler'",
        "'-DAPPLICATION_VIRTUALFILE_SUFFIX:STRING=kdrive'",
        "'-DBIN_INSTALL_DIR:PATH=$path'",
        "'-DVFS_DIRECTORY:PATH=$vfsDir'",
        "'-DKDRIVE_THEME_DIR:STRING=$path/infomaniak'",
        "'-DPLUGINDIR:STRING=C:/Program Files (x86)/kDrive/lib/kDrive/plugins'",
        "'-DZLIB_INCLUDE_DIR:PATH=C:/Program Files (x86)/zlib-1.2.11/include'",
        "'-DZLIB_LIBRARY_RELEASE:FILEPATH=C:/Program Files (x86)/zlib-1.2.11/lib/zlib.lib'",
        "'-DAPPLICATION_NAME:STRING=kDrive'",
        "'-DKDRIVE_VERSION_BUILD=$buildVersion'"
    )

    if ($ci) {
        $flags += ("'-DBUILD_UNIT_TESTS:BOOL=TRUE'")
        $flags += ("'-DKD_COVERAGE:BOOL=TRUE'")
    }

    $args += $flags

    $args += ("'-B$buildPath'")
    $args += ("'-H$path'")
    $args += ("'-DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake'")

    $cmake = ('cmake {0}' -f ($args -Join ' '))

    Write-Host $cmake
    Invoke-Expression $cmake

    $buildArgs += @('--build', $buildPath, '--target all install')
    $buildCall = ('cmake {0}' -f ($buildArgs -Join ' '))

    Write-Host "Building and installing executables with CMake ..."

    Write-Host $buildCall
    Invoke-Expression $buildCall

    Write-Host "CMake build done."
}

function Get-Icon-Path {
    param (
        [string] $buildPath
    )

    # NSIS needs the path to use backslash
    $iconPath = "$buildPath\src\gui\kdrive-win.ico".Replace('/', '\')

    return $iconPath
}

function Set-Up-NSIS {
    param (
        [string] $buildPath,
        [string] $contentPath,
        [string] $extPath,
        [string] $vfsDir,
        [string] $archiveName,
        [string] $archivePath,
        [string] $archiveDataPath,
        [bool] $upload
    )

    Write-Host "Setting up NSIS."

    # NSIS needs the path to use backslash
    $iconPath = Get-Icon-Path $buildpath
    $appName = Get-App-Name $buildpath
   
    $installerPath = Get-Installer-Path $buildPath $contentPath
    Clean $installerPath

    $aumid = Get-Aumid $upload
    $prodName = "kDrive"
    $compName = "Infomaniak Network SA"
    $version = (Select-String -Path $buildPath\version.h KDRIVE_VERSION_FULL | foreach-object { $data = $_ -split " "; echo $data[3] })

    $scriptContent = Get-Content "$buildPath/NSIS.template.nsi" -Raw
    $scriptContent = $scriptContent -replace "@{icon}", $iconPath
    $scriptContent = $scriptContent -replace "@{extpath}", $extPath
    $scriptContent = $scriptContent -replace "@{installerIcon}", "!define MUI_ICON $iconPath"
    $scriptContent = $scriptContent -replace "@{company}", $compName
    $scriptContent = $scriptContent -replace "@{productname}", $prodName
    $scriptContent = $scriptContent -replace "@{nsis_include_internal}", '!addincludedir "C:\Program Files (x86)\NSIS"'
    $scriptContent = $scriptContent -replace "@{nsis_include}", "!addincludedir $buildPath"
    $scriptContent = $scriptContent -replace "@{dataPath}", $archiveDataPath
    $scriptContent = $scriptContent -replace "@{dataName}", $archiveName
    $scriptContent = $scriptContent -replace "@{setupname}", "$appName"
    $scriptContent = $scriptContent -replace "@{7za}", $7ZaPath
    $scriptContent = $scriptContent -replace "@{version}", $version
    $scriptContent = $scriptContent -replace "@{AUMID}", $aumid
    Set-Content -Path "$buildPath/NSIS.template.nsi" -Value $scriptContent

    Write-Host "NSIS is set up."
}

function Prepare-Archive {
    param (
        [string] $buildType,
        [string] $buildPath,
        [string] $vfsDir,
        [string] $archivePath,
        [bool] $upload
    )

    Write-Host "Preparing the archive ..."

    $dependencies = @(
        "${env:ProgramFiles(x86)}/zlib-1.2.11/bin/zlib1",
        "${env:ProgramFiles(x86)}/libzip/bin/zip",
        "${env:ProgramFiles(x86)}/log4cplus/bin/log4cplusU",
        "${env:ProgramFiles}/OpenSSL/bin/libcrypto-3-x64",
        "${env:ProgramFiles}/OpenSSL/bin/libssl-3-x64",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoCrypto",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoFoundation",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoJSON",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoNet",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoNetSSL",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoUtil",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoXML",
        "${env:ProgramFiles(x86)}/Sentry-Native/bin/sentry",
        "${env:ProgramFiles(x86)}/xxHash/bin/xxhash",
        "$vfsDir/Vfs",
        "$buildPath/bin/kDrivecommonserver_vfs_win"
    )

    Write-Host "Copying dependencies to the folder $archivePath"
    foreach ($file in $dependencies) {
        if (($buildType -eq "Debug") -and (Test-Path -Path $file"d.dll")) {
            Copy-Item -Path $file"d.dll" -Destination "$archivePath"
        }
        else {
            Copy-Item -Path $file".dll" -Destination "$archivePath"
        }
    }

    Copy-Item -Path "$path/sync-exclude-win.lst" -Destination "$archivePath/sync-exclude.lst"

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($ci) {
        Write-Host "Archive prepared for CI build."
        exit 0
    }

    $iconPath = Get-Icon-Path $buildpath
    if (Test-Path -Path $iconPath) {
        Copy-Item -Path "$iconPath" -Destination $archivePath
    }

    $binaries = @(
        "${env:ProgramFiles(x86)}/Sentry-Native/bin/crashpad_handler.exe",
        "kDrive.exe",
        "kDrive_client.exe"
    )

    # Move each executable to the bin folder and sign them
    foreach ($file in $binaries) {
        if (Test-Path -Path $buildPath/bin/$file) {
            Copy-Item -Path "$buildPath/bin/$file" -Destination "$archivePath"
            & "$env:QTDIR\bin\windeployqt.exe" "$archivePath\$file"
        }
        else {
            Copy-Item -Path $file -Destination $archivePath
        }

        $filename = Split-Path -Leaf $file

        if (!$thumbprint) {
            $thumbprint = Get-Thumbprint $upload
        }

        & signtool sign /sha1 $thumbprint /fd SHA1 /t http://timestamp.digicert.com /v $archivePath/$filename
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        & signtool sign /sha1 $thumbprint /fd sha256 /tr http://timestamp.digicert.com?td=sha256 /td sha256 /as /v $archivePath/$filename
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    Write-Host "Archive prepared."
}

function Create-Archive {
    param (
        [string] $path,
        [string] $buildPath,
        [string] $contentPath,
        [string] $installPath,
        [string] $archiveName,
        [string] $archivePath,
        [bool] $upload
    )

    Write-Host "Creating the archive ..."

    $target = "$contentPath/$archiveName"
    $sourceTranslation = "$installPath/i18n"
    $sourceFiles = "$archivePath/*"

    7za a -mx=5 $target $sourceTranslation
    7za a -mx=5 $target $sourceFiles

    Copy-Item -Path "$path\cmake\modules\NSIS.InstallOptions.ini.in" -Destination "$buildPath/NSIS.InstallOptions.ini"
    Copy-Item -Path "$path\cmake\modules\NSIS.InstallOptionsWithButtons.ini.in" -Destination "$buildPath/NSIS.InstallOptionsWithButtons.ini"

    & makensis "$buildPath\NSIS.template.nsi"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $appName = Get-App-Name $buildPath
    Move-Item -Path "$buildPath\$appName" -Destination "$contentPath"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # Sign final installer
    if (!$thumbprint) {
        $thumbprint = Get-Thumbprint $upload
    }

    $installerPath = Get-Installer-Path $buildPath $contentPath

    if (Test-Path -Path $installerPath) {
        & signtool sign /sha1 $thumbprint /fd SHA1 /t http://timestamp.digicert.com /v $installerPath
        & signtool sign /sha1 $thumbprint /fd sha256 /tr http://timestamp.digicert.com?td=sha256 /td sha256 /as /v $installerPath
    }
    else {
        Write-Host ("$installerPath not found. Unable to sign final installer.") -f Red
        exit 1
    }

    Write-Host "Archive created."
}

#################################################################################################
#                                                                                               #
#                                           COMMANDS                                            #
#                                                                                               #
#################################################################################################

$msbuildPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -version [16.0, 17.0] -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
$7zaPath = "${env:ProgramFiles}\7-Zip\7za.exe"

Set-Alias msbuild $msbuildPath
Set-Alias 7za $7zaPath

#################################################################################################
#                                                                                               #
#                                           PARAMETERS                                          #
#                                                                                               #
#################################################################################################

if ($help) {
    Write-Host ("
    Infomaniak kDrive - Desktop
    Copyright (C) 2023-2024 Infomaniak Network SA
   
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


This script will build the kDrive application according to the CMakeLists.txt file located in the root folder of the project.
A folder will be created at the root of the project directory named after the current Configuration to hold all the files.
For example, should you run the script with 'Release' as buildType, a folder named build-Release will be created.
Inside, you will find the build folder, install folder, and kDrive installer.

Parameters :
    `t-buildType`t: The configuration requested for the app, either Debug or Release (will default to Release)
    `t-path`t`t: The path to the root of the kdrive project folder (will default to the current directory)
    `t-thumprint`t: Set a specific certificate thumbprint to sign the executables
    `t-clean`t`t: Optional parameter for files cleaning. The following are available :
    `t`tbuild`t`t: Remove all the built files, located in '$path/build-$buildType/build', then exit the script
    `t`text`t`t`t: Remove the extension files, located in '$vfsDir', then exit the script
    `t`tall`t`t`t: Remove all the files, located in '$path/build-$buildType', then exit the script
    `t`tremake`t`t: Remove all the files, then rebuild the project
    `t-ext`t`t`t: Rebuild and redeploy the windows extension
    `t-upload`t`t: Upload flag to switch between the virtual and physical certificates. Also rebuilds the project
    ") -f Cyan

    Write-Host ("It is mandatory that all dependencies are already built and installed before building.
To run this script, you will need to call it from the Native Tools Command Prompt for VS.
Alternatively, you can run vcvars64.bat from your command prompt (usually located in C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build)
This is required to make the compiler work with CMake.
To avoid issues with the NSIS packaging, please use NSIS version 3.03.
The installer packages are first compressed using 7za.exe, you will need to have it installed") -f Yellow
    
    exit
}

Clean $archiveDataPath

switch -regex ($clean.ToLower()) {
    "b(uild)?" {
        Write-Host "Removing kDrive built files" -f Yellow
        Clean $buildPath
        Write-Host "Done, exiting" -f Yellow
        exit 
    }

    "ext" {
        Write-Host "Removing extension files" -f Yellow
        Clean $vfsDir
        Write-Host "Done, exiting" -f Yellow
        exit
    }

    "all" {
        Write-Host "Removing all generated files" -f Yellow 
        Clean $contentPath
        Clean $vfsDir
        Write-Host "Done, exiting" -f Yellow
        exit
    }

    "re(make)?" {
        Write-Host "Removing all generated files" -f Yellow
        Clean $contentPath
        Clean $vfsDir
        Write-Host "Done, rebuilding" -f Yellow
    }
    default {}
}

if (!$thumbprint) {
    $thumbprint = Get-Thumbprint $upload
}

if ($upload) {
    Write-Host "You are about to build kDrive for an upload"
    Write-Host "Once the build is complete, you will need to call the upload script"
    $confirm = Read-Host "Please make sure you set the correct certificate for the Windows extension. Continue ? (Y/n)"
    if (!($confirm -match "^y(es)?$")) {
        exit 1
    }

    Write-Host "Preparing for full upload build." -f Green
    Clean $contentPath
    Clean $vfsDir
}

#################################################################################################
#                                                                                               #
#                                           EXTENSION                                           #
#                                                                                               #
#################################################################################################

if (!(Test-Path "$vfsDir\vfs.dll") -or $ext) {
    Build-Extension $path $extPath $buildType

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to build the extension. Aborting." -f Red
        exit $LASTEXITCODE
    }
}

#################################################################################################
#                                                                                               #
#                                           CMAKE                   	                        #
#                                                                                               #
#################################################################################################

CMake-Build-And-Install $path $installPath $vfsDir

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake build failed. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                           NSIS SETUP                                          #
#                                                                                               #
#################################################################################################

Set-Up-NSIS $buildPath $contentPath $extPath $vfsDir $archiveName $archivePath $archiveDataPath $upload

if ($LASTEXITCODE -ne 0) {
    Write-Host "NSIS setup failed. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                       ARCHIVE PREPARATION                                     #
#                                                                                               #
#################################################################################################

Prepare-Archive $buildType $buildPath $vfsDir $archivePath $upload
{
    Write-Host "Archive preparation failed. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                       ARCHIVE CREATION                                        #
#                                                                                               #
#################################################################################################

if (!$ci) {
    Create-Archive $path $buildPath $contentPath $installPath $archiveName $archivePath $upload
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Archive creation failed. Aborting." -f Red
        exit $LASTEXITCODE
    }
}

#################################################################################################
#                                                                                               #
#                                              CLEAN-UP                                         #
#                                                                                               #
#################################################################################################

Copy-Item -Path "$buildPath\bin\kDrive*.pdb" -Destination $contentPath
Remove-Item $archiveDataPath

#################################################################################################
#                                                                                               #
#                                         UPLOAD INVITE                                         #
#                                                                                               #
#################################################################################################

if ($upload) {
    Write-Host "Packaging done."
    Write-Host "Run the upload script to generate the update file and upload the new version."
}
