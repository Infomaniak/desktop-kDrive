<#
 Infomaniak kDrive - Desktop App
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

# wo	: The path to the build wrapper output directory used by SonarCloud CI Analysis
[string] $wo= $null,

# Upload : Flag to trigger the use of the USB-key signing certificate
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

# NSIS needs the path to use backslash
$iconPath = "$buildPath\src\gui\kdrive-win.ico".Replace('/', '\')
$prodName = "kDrive"
$compName = "Infomaniak Network SA"

$QTDIR = $env:QTDIR

# Files to be added to the archive and then packaged
$archivePath = "$installPath/bin"
$archiveName = "kDrive.7z"

# NSIS needs the path to use backslash
$archiveDataPath = ('{0}\build-windows\{1}' -f $path.Replace('/', '\'), $archiveName)

$sourceTranslation = "$installPath/i18n"
$sourceFiles = "$archivePath/*"
$target = "$contentPath/$archiveName"

$buildVersion = Get-Date -Format "yyyyMMdd"
$aumid = if ($upload) {$env:KDC_PHYSICAL_AUMID} else {$env:KDC_VIRTUAL_AUMID}

#################################################################################################
#                                                                                               #
#                                           FUNCTIONS                                           #
#                                                                                               #
#################################################################################################

function Clean {
	param (
		[string] $cleanPath
	)

	if (Test-Path -Path $cleanPath)
	{
		Remove-Item -Path $cleanPath -Recurse
	}
}

function Get-Thumbprint {
   param (
		[bool] $upload
   )

   $thumbprint = 
   If ($upload)
   {
		Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match "Infomaniak" -and $_.Issuer -match "EV" } | Select -ExpandProperty Thumbprint
   } 
   Else
   {
		Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -match "Infomaniak" -and $_.Issuer -notmatch "EV" } | Select -ExpandProperty Thumbprint
   }
   return $thumbprint
}

#################################################################################################
#                                                                                               #
#                                           COMMANDS                                            #
#                                                                                               #
#################################################################################################

$msbuildPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe
$7zaPath = "${env:ProgramFiles}\7-Zip\7za.exe"

Set-Alias msbuild $msbuildPath
Set-Alias 7za $7zaPath

#################################################################################################
#                                                                                               #
#                                           PARAMETERS                                          #
#                                                                                               #
#################################################################################################

if ($help)
{
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
	`t-ci`t`t`t: Build with CI testing
	`t-wo`t`t`t: Optional parameter to wrap the build with SonarCloud build-wrapper tool (CI Analysis). Set the path to the build wrapper output directory. The wrapper will be used iff this path is not empty.
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

switch -regex ($clean.ToLower())
{
	"b(uild)?" 
	{
		Write-Host "Removing kDrive built files" -f Yellow
		Clean $buildPath
		Write-Host "Done, exiting" -f Yellow
		exit 
	}

	"ext"
	{
		Write-Host "Removing extension files" -f Yellow
		Clean $vfsDir
		Write-Host "Done, exiting" -f Yellow
		exit
	}

	"all"
	{
		Write-Host "Removing all generated files" -f Yellow 
		Clean $contentPath
		Clean $vfsDir
		Write-Host "Done, exiting" -f Yellow
		exit
	}

	"re(make)?"
	{
		Write-Host "Removing all generated files" -f Yellow
		Clean $contentPath
		Clean $vfsDir
		Write-Host "Done, rebuilding" -f Yellow
	}
	default {}
}

if (!$thumbprint)
{
	$thumbprint = Get-Thumbprint $upload
}

if ($upload)
{
	Write-Host "You are about to build kDrive for an upload"
	Write-Host "Once the build is complete, you will need to call the upload script"
	$confirm = Read-Host "Please make sure you set the correct certificate for the Windows extension. Continue ? (Y/n)"
	if (!($confirm -match "^y(es)?$"))
	{
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

if (!$aumid)
{
	Write-Host "The AUMID value could not be read from env.
				Exiting." -f Red
	exit 1
}

if (!(Test-Path "$vfsDir\vfs.dll") -or $ext)
{
	msbuild "$extPath\kDriveExt.sln" /p:Configuration=Release /p:Platform=x64 /p:PublishDir="$extPath\FileExplorerExtensionPackage\AppPackages\" /p:DeployOnBuild=true
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	
	Copy-Item -Path "$extPath\Vfs\..\Common\debug.h" -Destination "$path\src\server\vfs\win\."
	Copy-Item -Path "$extPath\Vfs\Vfs.h" -Destination "$path\src\server\vfs\win\."
}

#################################################################################################
#                                                                                               #
#                                           CMAKE                   	                        #
#                                                                                               #
#################################################################################################

$msvc_bin_path = "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Tools/MSVC/14.29.30133/bin"
$compiler_path = "$msvc_bin_path/Hostx64/x64/cl.exe"

$args = @("'-GNinja'")
$args += ("'-DCMAKE_BUILD_TYPE=$buildType'")
$args += ("'-DCMAKE_INSTALL_PREFIX=$installPath'")
$args += ("'-DCMAKE_PREFIX_PATH=$installPath'")

$flags = @(
"'-DCMAKE_MAKE_PROGRAM=C:\Qt\Tools\Ninja\ninja.exe'",
"'-DQT_QMAKE_EXECUTABLE:STRING=C:\Qt\Tools\CMake_64\bin\cmake.exe'",
"'-DCMAKE_C_COMPILER:STRING=$compiler_path'",
"'-DCMAKE_CXX_COMPILER:STRING=$compiler_path'",
"'-DAPPLICATION_UPDATE_URL:STRING=https://www.infomaniak.com/drive/update/desktopclient'",
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

if ($ci)
{
	$flags += ("'-DBUILD_UNIT_TESTS:BOOL=TRUE'")
}

$args += $flags

$args += ("'-B$buildPath'")
$args += ("'-H$path'")

$cmake = ('cmake {0}'-f($args -Join ' '))

Write-Host $cmake
Invoke-Expression $cmake

$buildArgs += @('--build', $buildPath, '--target all install')
$buildCall = ('cmake {0}' -f ($buildArgs -Join ' '))

if ($wo) { 	# Insert the SonarCloud build-wrapper tool for CI Analysis
	$buildCall = "build-wrapper-win-x86-64 --out-dir $wo $buildCall"
}

Write-Host $buildCall
Invoke-Expression $buildCall

if ($LASTEXITCODE -ne 0)
{
	Write-Host "CMake failed to build the source files. Aborting." -f Red
	exit $LASTEXITCODE
}

#################################################################################################
#                                                                                               #
#                                           NSIS SETUP                                          #
#                                                                                               #
#################################################################################################

$version = (Select-String -Path $buildPath\version.h KDRIVE_VERSION_FULL | foreach-object { $data = $_ -split " "; echo $data[3]})
$versionPart = $version -split "\."
$mainVersion = "$($versionPart[0]).$($versionPart[1]).$($versionPart[2])"
$buildVersion = $versionPart[3]

$appName = "$prodName-$version.exe"
$installerPath = "$contentPath/$appName"

Clean $installerPath

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

#################################################################################################
#                                                                                               #
#                                       ARCHIVE PREPARATION                                     #
#                                                                                               #
#################################################################################################

$binaries = @(
"${env:ProgramFiles(x86)}/Sentry-Native/bin/crashpad_handler.exe",
"kDrive.exe",
"kDrive_client.exe"
)

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
"$buildPath/bin/kDrivesyncengine_vfs_win"
)

Write-Host "Copying dependencies to the folder $archivePath"
foreach ($file in $dependencies)
{
	if (($buildType -eq "Debug") -and (Test-Path -Path $file"d.dll")) {
		Copy-Item -Path $file"d.dll" -Destination "$archivePath"
	}
	else {
		Copy-Item -Path $file".dll" -Destination "$archivePath"
	}
}

Copy-Item -Path "$path/sync-exclude-win.lst" -Destination "$archivePath/sync-exclude.lst"

if ($ci)
{
	exit $LASTEXITCODE
}

if (Test-Path -Path $iconPath)
{
	Copy-Item -Path "$iconPath" -Destination $archivePath
}

# Move each executable to the bin folder and sign them
foreach ($file in $binaries)
{
	if (Test-Path -Path $buildPath/bin/$file) {
		Copy-Item -Path "$buildPath/bin/$file" -Destination "$archivePath"
		& "$QTDIR\bin\windeployqt.exe" "$archivePath\$file"
	}
	else {
		Copy-Item -Path $file -Destination $archivePath
	}

	$filename = Split-Path -Leaf $file

	& signtool sign /sha1 $thumbprint /fd SHA1 /t http://timestamp.comodoca.com /v $archivePath/$filename
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
	& signtool sign /sha1 $thumbprint /fd sha256 /tr http://timestamp.comodoca.com?td=sha256 /td sha256 /as /v $archivePath/$filename
	if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

#################################################################################################
#                                                                                               #
#                                       ARCHIVE CREATION                                        #
#                                                                                               #
#################################################################################################

7za a -mx=5 $target $sourceTranslation
7za a -mx=5 $target $sourceFiles

Copy-Item -Path "$path\cmake\modules\NSIS.InstallOptions.ini.in" -Destination "$buildPath/NSIS.InstallOptions.ini"
Copy-Item -Path "$path\cmake\modules\NSIS.InstallOptionsWithButtons.ini.in" -Destination "$buildPath/NSIS.InstallOptionsWithButtons.ini"

& makensis "$buildPath\NSIS.template.nsi"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Move-Item -Path "$buildPath\$appName" -Destination "$contentPath"
# Sign final installer
if (Test-Path -Path $installerPath)
{
	& signtool sign /sha1 $thumbprint /fd SHA1 /t http://timestamp.comodoca.com /v $installerPath
	& signtool sign /sha1 $thumbprint /fd sha256 /tr http://timestamp.comodoca.com?td=sha256 /td sha256 /as /v $installerPath
}
else
{
	Write-Host ("$installerPath not found. Unable to sign final installer.") -f Red
	exit 1
}

#################################################################################################
#                                                                                               #
#                                              CLEAN-UP                                         #
#                                                                                               #
#################################################################################################

Copy-Item -Path "$buildPath\bin\kDrive*.pdb" -Destination $contentPath
Remove-Item $archiveDataPath

if ($upload)
{
	Write-Host "Packaging done"
	Write-Host "Run the upload script to generate the update file and upload the new version"
}
