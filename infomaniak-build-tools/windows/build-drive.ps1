<#
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

    # Path: The path to the root CMakeLists.txt
    [string] $path = $PWD.Path,

    # Clean: The files to clean on execution
    [string] $clean,

    # Ext: Rebuild the extension (automatically done if vfs.h is missing)
    [switch] $ext,

    # Ci: Build configured for CI
    [switch] $ci,

    # Upload: Flag to trigger the use of the USB-key signing certificate
    [switch] $upload,

    # tokenPass: The password to use for unlocking the USB-key signing certificate (only used if upload is set)
    [String] $tokenPass,
	
	# Msi: Build MSI installer
    [switch] $msi,

    # Coverage: Flag to enable or disable the code coverage computation
    [switch] $coverage,

    # Unit tests: Flag to enable or disable the build of unit tests
    [switch] $unitTests,

    # Help: Displays the help message and exits
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

$extPath = "$path/extensions/windows/cfapi"
$vfsDir = "$extPath/x64/Release"

$msiInstallerFolderPath = "$path/installer/windows/kDriveInstaller"
$msiPackageFolderPath = "$msiInstallerFolderPath/bin/x64/Release/en-US"

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

function Set-Bullseye-Coverage {
    param (
        [bool] $enabled
    )

    $cov01Parameter ="-0"
    if ($enabled) {
        $cov01Parameter = "-1"
        # Without the next line, CMake may not wrap the compiler.
        $env:Path="$env:Programfiles\BullseyeCoverage\bin;$env:Path"
    }

    try {
        $cmd = Get-Command cov01.exe -ErrorAction Stop
        & $cmd $cov01Parameter
     } catch {
         Write-Host "BullseyeCoverage cov01.exe command not found."
         if ($enable) {
            exit 1
         }
     }
     
     $outputString = "disabled"
     if ($enabled) {
        $outputString = "enabled"
    }

     Write-Host "BullseyeCoverage is $outputString."
} 
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
        [bool] $upload,
        [bool] $ci # On CI build machines, the certificate are located in local computer store
    )
    if ($ci) {
        $certStore = "Cert:\LocalMachine\My"
    } else {
        $certStore = "Cert:\CurrentUser\My"
    }
    
    $thumbprint = 
    If ($upload) {
         Get-ChildItem $certStore | Where-Object { $_.Subject -match "Infomaniak" -and $_.Issuer -match "EV" } | Select -ExpandProperty Thumbprint
    } 
    Else {
        Get-ChildItem $certStore | Where-Object { $_.Subject -match "Infomaniak" -and $_.Issuer -notmatch "EV" } | Select -ExpandProperty Thumbprint
    }
    Write-Host "Using thumbprint: $thumbprint"

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

function Get-Package-Name {
    param (
        [string] $buildPath,
        [switch] $msi,
		[switch] $exe
    )

    $prodName = "kDrive"
    $version = (Select-String -Path $buildPath\version.h KDRIVE_VERSION_FULL | foreach-object { $data = $_ -split " "; echo $data[3] })
	if ($msi) {
		$appName = "$prodName-$version.msi"
	} ElseIf ($exe) {
		$appName = "$prodName-$version.exe"
	} else {
		$appName = "$prodName-$version"
	}

    return $appName
}

function Get-Installer-Path {
    param (
        [string] $buildPath,
        [string] $contentPath,
		[switch] $msi
    )

	if ($msi) {
		$appName = Get-Package-Name $buildPath -msi
	} else {
		$appName = Get-Package-Name $buildPath -exe
	}
    
    $installerPath = "$contentPath/$appName"

    return $installerPath
}

function Build-Extension {
    param (
        [string] $path,
        [string] $contentPath,
        [string] $extPath,
        [string] $buildType,
        [string] $thumbprint
    )

    Write-Host "Building extension ($buildType) ..."

    $configuration = $buildType
    if ($buildType -eq "RelWithDebInfo") { $configuration = "Release" }
    if($upload) {
        $publisher = "CN=Infomaniak Network SA, O=Infomaniak Network SA, S=Genève, C=CH, OID.2.5.4.15=Private Organization, OID.1.3.6.1.4.1.311.60.2.1.3=CH, SERIALNUMBER=CHE-103.167.648"
    }else{
        $publisher = "CN=INFOMANIAK NETWORK SA, O=INFOMANIAK NETWORK SA, S=Genève, C=CH"
    }

    $appxManifestPath = "$extPath\FileExplorerExtensionPackage\Package.appxmanifest"
    if (Test-Path $appxManifestPath) {
        (Get-Content $appxManifestPath -Raw) -replace 'Publisher="[^"]*"', "Publisher=`"$publisher`"" |
        Set-Content -Encoding UTF8 -Force $appxManifestPath
    } else {
        Write-Host "Package.appxmanifest not found at $appxManifestPath" -ForegroundColor Red
        exit 1
    }
    Write-Host "Publisher set to: $publisher" -ForegroundColor Yellow

        
    $map = @{}
    Select-String -Path ".\VERSION.cmake" -Pattern 'set\( *KDRIVE_VERSION_(MAJOR|MINOR|PATCH) *(\d+)' | ForEach-Object {
        if ($_ -match 'KDRIVE_VERSION_(MAJOR|MINOR|PATCH)\s+(\d+)') {
            $map[$matches[1]] = [int]$matches[2]
        }
    }

    $version = "$($map['MAJOR']).$($map['MINOR']).$($map['PATCH'])"
    (Get-Content $appxManifestPath -Raw) -replace ' Version="[^"]*" />', " Version=`"$version.0`" />" |
        Set-Content -Encoding UTF8 -Force $appxManifestPath
    Write-Host "Extension version: $version"
	
	  $aumid = Get-Aumid $upload
	  Write-Host "Building extension with AUMID: $aumid"
	
    msbuild "$extPath\kDriveExt.sln" /p:Configuration=$configuration /p:Platform=x64 /p:PublishDir="$extPath\FileExplorerExtensionPackage\AppPackages\" /p:DeployOnBuild=true /p:PackageCertificateThumbprint="$thumbprint" /p:KDC_AUMID="$aumid"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $bundlePath = "$extPath/FileExplorerExtensionPackage/AppPackages/FileExplorerExtensionPackage_$version.0_Test/FileExplorerExtensionPackage_$version.0_x64_arm64.msixbundle"
    Sign-File -FilePath $bundlePath -Upload $upload -Thumbprint $thumbprint -TokenPass $tokenPass -Description "FileExplorerExtensionPackage"

    $srcVfsPath = "$path/src/libcommonserver/vfs/win/."
    Copy-Item -Path "$extPath/Vfs/../Common/debug.h" -Destination $srcVfsPath
    Copy-Item -Path "$extPath/Vfs/Vfs.h" -Destination $srcVfsPath

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # Create a copy for NSIS.template.nsi.in where paths are shorter (long paths can cause the NSIS `File` function to fail).
    Copy-Item -Path "$extPath/FileExplorerExtensionPackage/AppPackages/FileExplorerExtensionPackage_$version.0_Test" -Destination "$contentPath/vfs_appx_directory" -Recurse

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "Extension built."
}

function CMake-Build-And-Install {
    param (
        [string] $path,
        [string] $installPath,
        [string] $vfsDir,
        [bool] $ci
    )
    Write-Host "1) Installing Conan dependencies…"
    $conanFolder = Join-Path $buildPath "conan"
    # mkdir -p this folder
    if (-not (Test-Path $conanFolder)) {
        New-Item -Path $conanFolder -ItemType Directory
    }
    Write-Host "Conan folder: $conanFolder"

    if ($ci) {
      & "$path\infomaniak-build-tools\conan\build_dependencies.ps1" Release -OutputDir $conanFolder -Ci -MakeRelease
    } else {
      & "$path\infomaniak-build-tools\conan\build_dependencies.ps1" Release -OutputDir $conanFolder -MakeRelease
    }

    
    $conanToolchainFile = Get-ChildItem -Path $conanFolder -Filter "conan_toolchain.cmake" -Recurse -File |
            Select-Object -ExpandProperty FullName -First 1

    if (-not (Test-Path $conanToolchainFile)) {
        Write-Error "Conan toolchain file not found. Abort."
        exit 1
    }
    Write-Host "Conan toolchain file used: $conanToolchainFile"

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
        "'-DCMAKE_TOOLCHAIN_FILE=$conanToolchainFile'",
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

    if ($unitTests) {
        $flags += ("'-DBUILD_UNIT_TESTS:BOOL=TRUE'")
    }

    if ($coverage) {
        $flags += ("'-DKD_COVERAGE:BOOL=TRUE'")
    }

    $args += $flags

    $args += ("'-B$buildPath'")
    $args += ("'-H$path'")

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
    $appName = Get-Package-Name $buildpath -exe
   
    $installerPath = Get-Installer-Path $buildPath $contentPath
    Clean $installerPath

    $aumid = Get-Aumid $upload
    $prodName = "kDrive"
    $compName = "Infomaniak Network SA"
    $version = (Select-String -Path $buildPath\version.h KDRIVE_VERSION_FULL | foreach-object { $data = $_ -split " "; echo $data[3] })

    $scriptContent = Get-Content "$buildPath/NSIS.template.nsi" -Raw
    $scriptContent = $scriptContent -replace "@{icon}", $iconPath
    $scriptContent = $scriptContent -replace "@{vfs_appx_directory}", "$contentPath\vfs_appx_directory"
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

function Sign-File {
    param (
        [string] $filePath,
        [bool] $upload = $false,
        [string] $thumbprint,
        [string] $tokenPass = "",
		[string] $description = ""
    )
    Write-Host "Signing the file $filePath with thumbprint $thumbprint" -f Yellow
    & "$path\infomaniak-build-tools\windows\ksigntool.exe" sign /sha1 $thumbprint /tr http://timestamp.digicert.com?td=sha256 /fd sha256 /td sha256 /v /debug /sm /d $description $filePath /password:$tokenPass
    $res = $LASTEXITCODE
    Write-Host "Signing exit code: $res" -ForegroundColor Yellow
    if ($res -ne 0) {
        Write-Host "Signing failed with exit code $res" -ForegroundColor Red
        exit $res
    }
    else {
        Write-Host "Signing successful." -ForegroundColor Green
    }
}

function Prepare-Archive {
    param (
        [string] $buildType,
        [string] $buildPath,
        [string] $vfsDir,
        [string] $archivePath,
        [bool] $upload,
        [bool] $ci
    )

    Write-Host "Preparing the archive ..."

    $dependencies = @(
        "${env:ProgramFiles(x86)}/zlib-1.2.11/bin/zlib1",
        "${env:ProgramFiles(x86)}/libzip/bin/zip",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoCrypto",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoFoundation",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoJSON",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoNet",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoNetSSL",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoUtil",
        "${env:ProgramFiles(x86)}/Poco/bin/PocoXML",
        "${env:ProgramFiles(x86)}/Sentry-Native/bin/sentry",
        "$vfsDir/Vfs",
        "$buildPath/bin/kDrive_vfs_win"
    )

    Write-Host "Copying dependencies to the folder $archivePath"
    foreach ($file in $dependencies) {
        if (($buildType -eq "Debug") -and (Test-Path -Path $file"d.dll")) {
            Copy-Item -Path "${file}d.dll" -Destination "$archivePath"
        } else {
            Copy-Item -Path "$file.dll" -Destination "$archivePath"
        }
    }
    $find_dep_script = "$path/infomaniak-build-tools/conan/find_conan_dep.ps1"
    $packages = @(
        @{ Name = "xxhash";    Dlls = @("xxhash") },
        @{ Name = "log4cplus"; Dlls = @("log4cplus") },
        @{ Name = "openssl";   Dlls = @("libcrypto-3-x64", "libssl-3-x64") }
    )

    foreach ($pkg in $packages) {
        $args = @{ Package = $pkg.Name; BuildDir = $buildPath }
        $binFolder = & $find_dep_script @args
        foreach ($dll in $pkg.Dlls) {
            if (($buildType -eq "Debug") -and (Test-Path -Path $file"d.dll")) {
                Copy-Item -Path "$binFolder/${dll}d.dll" -Destination "$archivePath"
            } else {
                Copy-Item -Path "$binFolder/$dll.dll" -Destination "$archivePath"
            }
        }
    }

    Copy-Item -Path "$path/sync-exclude-win.lst" -Destination "$archivePath/sync-exclude.lst"

    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if (!$upload) {
        Write-Host "Archive prepared. Skipping binaries required for upload only."
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

        $thumbprint = Get-Thumbprint -Upload $upload -Ci $ci
        Sign-File -FilePath $archivePath/$filename -Upload $upload -Thumbprint $thumbprint -TokenPass $tokenPass -Description $filename

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
        [bool] $upload,
        [bool] $ci
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

    $appName = Get-Package-Name $buildPath -exe
    Move-Item -Path "$buildPath\$appName" -Destination "$contentPath"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # Sign final installer
    $thumbprint = Get-Thumbprint -Upload $upload -Ci $ci
    $installerPath = Get-Installer-Path $buildPath $contentPath

    if (Test-Path -Path $installerPath) {
        Sign-File -FilePath $installerPath -Upload $upload -Thumbprint $thumbprint -TokenPass $tokenPass -Description $appName
        Write-Host ("$installerPath signed successfully.") -f Green
    }
    else {
        Write-Host ("$installerPath not found. Unable to sign final installer.") -f Red
        exit 1
    }

    Write-Host "Archive created."
}

function Create-MSI-Package {
    Write-Host "Creating MSI package ..."

	$appName = Get-Package-Name $buildPath
	
	dotnet build "$msiInstallerFolderPath/kDriveInstaller.sln" /p:Configuration="Release" /p:Platform="x64" /p:OutputName=$appName
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

	Move-Item -Path "$msiPackageFolderPath/$appName.msi" -Destination "$contentPath"
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

	# Sign final installer
	if (!$thumbprint) {
		$thumbprint = Get-Thumbprint $upload
	}

	$installerPath = Get-Installer-Path $buildPath $contentPath -msi

	if (Test-Path -Path $installerPath) {
		Sign-File -FilePath $installerPath -Upload $upload -Thumbprint $thumbprint -TokenPass $tokenPass -Description $appName
		Write-Host ("$installerPath signed successfully.") -f Green
	}
	else {
		Write-Host ("$installerPath not found. Unable to sign final installer.") -f Red
		exit 1
	}

	Write-Host "MSI package created."
}

#################################################################################################
#                                                                                               #
#                                           COMMANDS                                            #
#                                                                                               #
#################################################################################################

$msbuildPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" "-version" "[16.0, 17.0]" "-products" "*" "-requires" "Microsoft.Component.MSBuild" "-find" "MSBuild\**\Bin\MSBuild.exe"
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
    `t-clean`t`t: Optional parameter for files cleaning. The following are available :
    `t`tbuild`t`t: Remove all the built files, located in '$path/build-$buildType/build', then exit the script
    `t`text`t`t`t: Remove the extension files, located in '$vfsDir', then exit the script
    `t`tall`t`t`t: Remove all the files, located in '$path/build-$buildType', then exit the script
    `t`tremake`t`t: Remove all the files, then rebuild the project
    `t-ext`t`t`t: Rebuild and redeploy the windows extension
    `t-ci`t`t`t: Use the CI build configuration
    `t-upload`t`t: Upload flag to switch between the virtual and physical certificates. Also rebuilds the project
    `t-coverage`t`t: Enable coverage computation
    `t-unitTests`t`t: Enable unit tests build
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

Write-Host
Write-Host "Build of type $buildType."
Write-Host

if ($upload) {
    Write-Host "You are about to build kDrive for an upload"
    Write-Host "Once the build is complete, you will need to call the upload script"
    #$confirm = Read-Host "Please make sure you set the correct certificate for the Windows extension. Continue ? (Y/n)"
    #if (!($confirm -match "^y(es)?$")) {
    #    exit 1
    #}

    Write-Host "Preparing for full upload build." -f Green
    Clean $contentPath
    Clean $vfsDir
}

#################################################################################################
#                                                                                               #
#                                           COVERAGE                                            #
#                                                                                               #
#################################################################################################

Set-Bullseye-Coverage $coverage
Write-Host

if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to enable code coverage computation. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                           EXTENSION                                           #
#                                                                                               #
#################################################################################################

if (!(Test-Path "$vfsDir\vfs.dll") -or $ext) {
    $thumbprint = Get-Thumbprint -Upload $upload -Ci $ci
    Build-Extension -Path $path -ContentPath $contentPath -ExtPath $extPath -BuildType $buildType -Thumbprint $thumbprint

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

CMake-Build-And-Install -Path $path -InstallPath $installPath -VfsDir $vfsDir -Ci $ci

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake build failed. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                           NSIS SETUP                                          #
#                                                                                               #
#################################################################################################

Set-Up-NSIS -BuildPath $buildPath -ContentPath $contentPath -ExtPath $extPath -VfsDir $vfsDir -ArchiveName $archiveName -ArchivePath $archivePath -ArchiveDataPath $archiveDataPath -Upload $upload

if ($LASTEXITCODE -ne 0) {
    Write-Host "NSIS setup failed. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                       ARCHIVE PREPARATION                                     #
#                                                                                               #
#################################################################################################

Prepare-Archive -BuildType $buildType -BuildPath $buildPath -VfsDir $vfsDir -ArchivePath $archivePath -Upload $upload -Ci $ci
if ($LASTEXITCODE -ne 0)
{
    Write-Host "Archive preparation failed. Aborting." -f Red
    exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                       ARCHIVE CREATION                                        #
#                                                                                               #
#################################################################################################

if (!$ci -or $upload) {
    Create-Archive -Path $path -BuildPath $buildPath -ContentPath $contentPath -InstallPath $installPath -Archivename $archiveName -ArchivePath $archivePath -Upload $upload -Ci $ci
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Archive creation failed ($LASTEXITCODE) . Aborting." -f Red
        exit $LASTEXITCODE
    }
}

#################################################################################################
#                                                                                               #
#                                     MSI PACKAGE CREATION                                      #
#                                                                                               #
#################################################################################################

if ($msi) {
    Create-MSI-Package
    if ($LASTEXITCODE -ne 0) {
        Write-Host "MSI package creation failed ($LASTEXITCODE) . Aborting." -f Red
        exit $LASTEXITCODE
    }
}


#################################################################################################
#                                                                                               #
#                                              CLEAN-UP                                         #
#                                                                                               #
#################################################################################################

Copy-Item -Path "$buildPath\kDrive*.pdb" -Destination $contentPath
Remove-Item $archiveDataPath

#################################################################################################
#                                                                                               #
#                                         UPLOAD INVITE                                         #
#                                                                                               #
#################################################################################################

if (!$ci -or $upload) {
    Write-Host "Packaging done."
    Write-Host "Run the upload script to generate the update file and upload the new version."
}
