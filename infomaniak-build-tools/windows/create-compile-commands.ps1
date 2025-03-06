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
# BuildType	: The type of build
[ValidateSet('Release', 'RelWithDebInfo', 'Debug')]
[string] $buildType = "RelWithDebInfo",

# Path	: The path to the root CMakeLists.txt directory
[string] $path = $PWD.Path,


# Help	: Displays the help message then exits if called
[switch] $help
)

#################################################################################################
#                                                                                               #
#                                       PATHS AND VARIABLES                                     #
#                                                                                               #
#################################################################################################

# CMake will treat any backslash as escape character and return an error
$path = $path.Replace('\', '/')

$contentPath = "$path/build-compile-commands"
$extPath = "$path/extensions/windows/cfapi/"
$vfsDir = $extPath + "x64/Release"

$buildPath = "$contentPath/build"
$installPath = "$contentPath/install"

$buildVersion = Get-Date -Format "yyyyMMdd"
$aumid = if ($upload) {$env:KDC_PHYSICAL_AUMID} else {$env:KDC_VIRTUAL_AUMID}


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
	") -f Cyan

	exit
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
	
	Copy-Item -Path "$extPath\Vfs\..\Common\debug.h" -Destination "$path\src\libcommonserver\vfs\win\."
	Copy-Item -Path "$extPath\Vfs\Vfs.h" -Destination "$path\src\libcommonserver\vfs\win\."
}

#################################################################################################
#                                                                                               #
#                                           CMAKE                   	                        #
#                                                                                               #
#################################################################################################

Write-Host "Creating compile commands ..."

$compiler = "cl.exe"

$args = @("'-GNinja'")
$args += ("'-DCMAKE_BUILD_TYPE=$buildType'")
$args += ("'-DCMAKE_INSTALL_PREFIX=$installPath'")
$args += ("'-DCMAKE_PREFIX_PATH=$installPath'")
$args += ("'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON'")

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


$flags += ("'-DBUILD_UNIT_TESTS:BOOL=TRUE'")
$flag += ("'-DCMAKE_MESSAGE_LOG_LEVEL:STRING=WARNING'")
$args += $flags

$args += ("'-B$buildPath'")
$args += ("'-H$path'")

$cmake = ('cmake {0}'-f($args -Join ' '))

Invoke-Expression $cmake  2>&1 | out-null

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Copy-Item "$buildPath/compile_commands.json" -Destination $path
Remove-Item -Path $contentPath -Recurse

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Compile commands successfully generared: '$path/compile_commands.json'"

exit $LASTEXITCODE
