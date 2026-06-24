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

# ----------------------------------------------------------------------------------------------
# Bootstrap script that provisions a fresh Windows development machine for the desktop-kDrive
# project, following infomaniak-build-tools/windows/Readme.md.
#
# Every step is idempotent and the script can be re-run safely (resume): already satisfied
# steps are reported as SKIPPED. See the -Help output for usage and examples.
# ----------------------------------------------------------------------------------------------

# Parameters :
Param(
    # ProjectsDir : Root folder that will hold the sources and the manually built dependencies.
    #               The README uses F:\Projects ; every path below is derived from this value.
    [string] $ProjectsDir = "C:\Projects",

    # Setup       : Full       => complete Visual Studio 2026 IDE (developer workstation)
    #               BuildTools => Visual Studio Build Tools only, no IDE (CI)
    [ValidateSet('Full', 'BuildTools')]
    [string] $Setup = 'Full',

    # Only        : Run only these steps (by name). See -Help for the list of step names.
    [string[]] $Only,

    # Skip        : Skip these steps (by name).
    [string[]] $Skip,

    # Clean       : Run the Clean action of each selected step instead of Install.
    [switch] $Clean,

    # Force       : Force the Install action even if the step is already satisfied.
    [switch] $Force,

    # Help        : Display the help message and exit.
    [switch] $Help
)

#################################################################################################
#                                                                                               #
#                                       PATHS AND VARIABLES                                     #
#                                                                                               #
#################################################################################################

# Everything is derived from $ProjectsDir ; never hardcode F:\ or C:\Projects below.
$script:ProjectsDir = $ProjectsDir
$script:RepoUrl     = "https://github.com/Infomaniak/desktop-kDrive.git"
$script:RepoDir     = Join-Path $ProjectsDir "desktop-kDrive"

# Manually built dependencies install locations (from the Windows Readme.md).
$script:CppUnitDir    = "C:\Program Files (x86)\cppunit"
$script:CppUnitSrcDir = Join-Path $ProjectsDir "cppunit"
$script:ZlibVersion   = "1.2.11"
$script:ZlibSrcDir    = Join-Path $ProjectsDir "zlib-$($script:ZlibVersion)"
$script:ZlibDir       = "C:\Program Files (x86)\zlib-$($script:ZlibVersion)"
$script:ZlibUrl       = "https://zlib.net/fossils/zlib-1.2.11.tar.gz"
$script:LibzipVersion = "v1.10.1"
$script:LibzipSrcDir  = Join-Path $ProjectsDir "libzip"
$script:LibzipDir     = "C:\Program Files (x86)\libzip"
$script:LibzipUrl     = "https://github.com/nih-at/libzip.git"
$script:NsisDir       = "C:\Program Files (x86)\NSIS"
$script:NsisUrl       = "https://downloads.sourceforge.net/project/nsis/NSIS%203/3.03/nsis-3.03-setup.exe"
$script:NsisPlugins   = @('LogicLib', 'nsProcess', 'UAC', 'x64')
$script:SevenZipDir   = "C:\Program Files\7-Zip"
$script:SevenZipUrl   = "https://downloads.sourceforge.net/project/sevenzip/7-Zip/23.01/7z2301-extra.7z"
$script:IcoutilsName  = "icoutils-0.32.3-x86_64"
$script:IcoutilsDir   = Join-Path $ProjectsDir $script:IcoutilsName
$script:IcoutilsUrl   = "https://downloads.sourceforge.net/project/unix-utils/icoutils/icoutils-0.32.3-x86_64.zip"

# Visual Studio 2026 winget package identifiers and workload / component identifiers.
# The list of product ids is available at https://learn.microsoft.com/en-us/visualstudio/install/workload-and-component-ids?view=visualstudio
# The list of workloads and components is available at https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-community?view=visualstudio&viewFallbackFrom=vs-2026&preserve-view=true
$script:VsPackageFull       = "Microsoft.VisualStudio.Product.Community"
$script:VsPackageBuildTools = "Microsoft.VisualStudio.Product.BuildTools"
# "Desktop development with C++" workload (IDE vs Build Tools flavour).
$script:VsWorkloadNativeIde   = "Microsoft.VisualStudio.Workload.NativeDesktop"
$script:VsWorkloadNativeTools = "Microsoft.VisualStudio.Workload.VCTools"
# "WinUI application development" workload.
$script:VsWorkloadWinUI = "Microsoft.VisualStudio.Workload.Universal"
# Individual components: Git for Windows and Windows 11 SDK (10.0.28000.x).
$script:VsComponentGit    = "Microsoft.VisualStudio.Component.Git"
$script:VsComponentWinSdk = "Microsoft.VisualStudio.Component.Windows11SDK.28000"

# Temporary download folder (kept under $ProjectsDir so nothing leaks outside the dev root).
$script:DownloadDir = Join-Path $ProjectsDir "_init-downloads"

# PATH entries that must be present for CMake to find the dependencies (from the Readme.md).
$script:PathEntries = @(
    (Join-Path $script:LibzipDir "bin"),
    (Join-Path $script:CppUnitDir "bin"),
    $script:NsisDir,
    (Join-Path $script:IcoutilsDir "bin")
)

# Accumulated per-step results, used to print the final summary table.
$script:Results = @()

#################################################################################################
#                                                                                               #
#                                       LOGGING HELPERS                                         #
#                                                                                               #
#################################################################################################

function Write-Section {
    param([string] $Message)
    Write-Host ""
    Write-Host "==== $Message ====" -ForegroundColor Cyan
}

function Write-Info  { param([string] $Message) Write-Host "  $Message" }
function Write-Ok    { param([string] $Message) Write-Host "  $Message" -ForegroundColor Green }
function Write-Warn  { param([string] $Message) Write-Host "  $Message" -ForegroundColor Yellow }
function Write-Err   { param([string] $Message) Write-Host "  $Message" -ForegroundColor Red }

#################################################################################################
#                                                                                               #
#                                       GENERIC HELPERS                                         #
#                                                                                               #
#################################################################################################

function Test-Administrator {
    # Returns $true when the current process is elevated.
    $identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Invoke-SelfElevation {
    # Attempts to relaunch the current script with administrator rights, preserving the
    # original arguments. The README requires the setup to run elevated.
    if (Test-Administrator) { return }

    Write-Warn "Administrator rights are required. Attempting auto-elevation..."
    try {
        $argList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSCommandPath`"")
        $argList += $script:BoundArgs
        Start-Process -FilePath "powershell.exe" -Verb RunAs -ArgumentList $argList -ErrorAction Stop
        Write-Info "An elevated instance has been started. This instance will now exit."
        exit 0
    } catch {
        Write-Err "Auto-elevation failed. Please re-run this script from an elevated (Administrator) prompt."
        exit 1
    }
}

function Get-File {
    # Downloads a file with Invoke-WebRequest and basic error handling. No secrets/tokens.
    param(
        [Parameter(Mandatory = $true)] [string] $Url,
        [Parameter(Mandatory = $true)] [string] $Destination
    )

    $dir = Split-Path -Parent $Destination
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }

    Write-Info "Downloading $Url"
    try {
        Invoke-WebRequest -Uri $Url -OutFile $Destination -UseBasicParsing -ErrorAction Stop
    } catch {
        throw "Download failed for '$Url': $($_.Exception.Message)"
    }

    if (-not (Test-Path $Destination)) {
        throw "Download did not produce the expected file '$Destination'."
    }

    # SourceForge and other mirrors occasionally answer with an HTML interstitial/error
    # page (HTTP 200) instead of the requested binary. The bogus file is then saved with
    # the expected name and only fails much later when executed (e.g. Windows reports
    # "The file or directory is corrupted and unreadable"). Detect that here so the step
    # fails immediately with an actionable message.
    if ((Get-Item $Destination).Length -eq 0) {
        throw "Download produced an empty file for '$Url'."
    }
    $stream = [System.IO.File]::OpenRead($Destination)
    try {
        $count = [System.Math]::Min(512, $stream.Length)
        $buffer = New-Object byte[] $count
        [void]$stream.Read($buffer, 0, $count)
    } finally {
        $stream.Dispose()
    }
    $prefix = [System.Text.Encoding]::ASCII.GetString($buffer)
    if ($prefix -match '(?i)^\s*(<!doctype html|<html|<\?xml)') {
        Remove-Item $Destination -Force -ErrorAction SilentlyContinue
        throw "Download for '$Url' returned an HTML page instead of the expected file (likely a broken or redirecting mirror)."
    }
}

function Get-VsWherePath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) { return $vswhere }
    return $null
}

function Test-VsWorkload {
    # Returns $true when an installed Visual Studio instance provides the given workload/component.
    param([Parameter(Mandatory = $true)] [string] $Requirement)

    $vswhere = Get-VsWherePath
    if (-not $vswhere) { return $false }

    $found = & $vswhere -products * -requires $Requirement -property installationPath 2>$null
    return [bool]$found
}

function Get-MachinePath {
    return [Environment]::GetEnvironmentVariable('Path', 'Machine')
}

function Test-InMachinePath {
    param([Parameter(Mandatory = $true)] [string] $Entry)
    $current = (Get-MachinePath) -split ';' | ForEach-Object { $_.TrimEnd('\') }
    return $current -contains $Entry.TrimEnd('\')
}

function Add-ToMachinePath {
    param([Parameter(Mandatory = $true)] [string] $Entry)
    if (Test-InMachinePath $Entry) { return }
    $current = Get-MachinePath
    $updated = ($current.TrimEnd(';') + ';' + $Entry)
    [Environment]::SetEnvironmentVariable('Path', $updated, 'Machine')
    # Also reflect the change in the current session.
    $env:Path = $env:Path.TrimEnd(';') + ';' + $Entry
    Write-Info "Added to machine PATH: $Entry"
}

function Remove-FromMachinePath {
    param([Parameter(Mandatory = $true)] [string] $Entry)
    if (-not (Test-InMachinePath $Entry)) { return }
    $kept = (Get-MachinePath) -split ';' | Where-Object { $_ -and ($_.TrimEnd('\') -ne $Entry.TrimEnd('\')) }
    [Environment]::SetEnvironmentVariable('Path', ($kept -join ';'), 'Machine')
    Write-Info "Removed from machine PATH: $Entry"
}

function Test-CommandExists {
    param([Parameter(Mandatory = $true)] [string] $Name)
    return [bool](Get-Command $Name -ErrorAction SilentlyContinue)
}

function Invoke-Native {
    # Runs an external command and throws when it returns a non-zero exit code.
    param(
        [Parameter(Mandatory = $true)] [string] $FilePath,
        [string[]] $Arguments = @(),
        [string] $WorkingDirectory
    )
    $previous = $null
    if ($WorkingDirectory) {
        $previous = (Get-Location).Path
        Set-Location $WorkingDirectory
    }
    try {
        # Stream both stdout and stderr straight to the console (the host) so the
        # external tool output is always visible and never silently captured by a caller.
        # Many tools (git, cmake, ...) write informational/progress text to stderr; merging
        # with 2>&1 wraps those lines as PowerShell ErrorRecords, which would otherwise be
        # rendered as noisy "NativeCommandError" blocks. Convert every item to plain text so
        # the output looks exactly like it would in a normal console, and so a stray stderr
        # line never aborts the step when $ErrorActionPreference is 'Stop'.
        & $FilePath @Arguments 2>&1 | ForEach-Object {
            if ($_ -is [System.Management.Automation.ErrorRecord]) {
                Write-Host $_.ToString()
            } else {
                Write-Host $_
            }
        }
        if ($LASTEXITCODE -ne 0) {
            throw "Command '$FilePath $($Arguments -join ' ')' failed with exit code $LASTEXITCODE."
        }
    } finally {
        if ($previous) { Set-Location $previous }
    }
}

# Tracks whether the MSVC x64 developer environment has already been imported this run.
$script:VsDevEnvLoaded = $false

function Enter-VsDeveloperEnvironment {
    # Imports the MSVC x64 toolchain (msbuild, nmake, cl, cmake, ...) into the current
    # PowerShell session, exactly like launching a 'x64 Native Tools Command Prompt' but
    # done programmatically so the script can run from an ordinary elevated PowerShell.
    # Idempotent: the environment is imported only once per run.
    if ($script:VsDevEnvLoaded) { return }

    $vswhere = Get-VsWherePath
    if (-not $vswhere) {
        throw "vswhere.exe not found. Visual Studio must be installed first (run the 'VisualStudio' step)."
    }

    # Prefer an instance that ships the C++ x64 toolchain, fall back to the latest instance.
    $installPath = & $vswhere -products * -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null | Select-Object -First 1
    if (-not $installPath) {
        $installPath = & $vswhere -products * -latest -property installationPath 2>$null | Select-Object -First 1
    }
    if (-not $installPath) {
        throw "No Visual Studio installation with the C++ toolchain was found. Run the 'VisualStudio' step first."
    }

    # Preferred path: the DevShell module shipped with Visual Studio.
    $devShellDll = Join-Path $installPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    if (Test-Path $devShellDll) {
        Import-Module $devShellDll
        Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
        $script:VsDevEnvLoaded = $true
        Write-Info "Loaded the Visual Studio x64 developer environment from $installPath"
        return
    }

    # Fallback: capture the environment produced by vcvars64.bat and import it.
    $vcvars = Join-Path $installPath "VC\Auxiliary\Build\vcvars64.bat"
    if (-not (Test-Path $vcvars)) {
        throw "Could not find Enter-VsDevShell or vcvars64.bat under $installPath. Reinstall the C++ workload (run the 'VisualStudio' step)."
    }
    $output = cmd /c "`"$vcvars`" >nul 2>&1 && set"
    foreach ($line in $output) {
        if ($line -match '^([^=]+)=(.*)$') {
            [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
        }
    }
    $script:VsDevEnvLoaded = $true
    Write-Info "Loaded the Visual Studio x64 developer environment (vcvars64) from $installPath"
}

#################################################################################################
#                                                                                               #
#                                          STEP HELPER                                          #
#                                                                                               #
#################################################################################################

function New-Step {
    # Builds the uniform step object exposing the three scriptblocks used by the engine.
    param(
        [Parameter(Mandatory = $true)] [string]      $Name,
        [Parameter(Mandatory = $true)] [string]      $Description,
        [Parameter(Mandatory = $true)] [scriptblock] $CheckIfSatisfied,
        [scriptblock] $Install = { },
        [scriptblock] $CleanAction = { },
        [switch] $ManualOnly,   # Interactive step: only detect and warn, never automate.
        [switch] $BuildToolsSkip # Skip this step when -Setup BuildTools (CI).
    )
    return [pscustomobject]@{
        Name             = $Name
        Description      = $Description
        CheckIfSatisfied = $CheckIfSatisfied
        Install          = $Install
        Clean            = $CleanAction
        ManualOnly       = [bool]$ManualOnly
        BuildToolsSkip   = [bool]$BuildToolsSkip
    }
}

#################################################################################################
#                                                                                               #
#                                            STEPS                                              #
#                                                                                               #
#################################################################################################

function Get-Steps {
    $steps = @()

    # 1. Prerequisites : elevation and $ProjectsDir creation.
    $steps += New-Step -Name "Prerequisites" -Description "Administrator rights and projects directory" `
        -CheckIfSatisfied {
            (Test-Administrator) -and (Test-Path $script:ProjectsDir)
        } `
        -Install {
            if (-not (Test-Administrator)) {
                throw "This script must be run as Administrator (the Windows README requires it). Re-run from an elevated prompt."
            }
            if (-not (Test-Path $script:ProjectsDir)) {
                New-Item -ItemType Directory -Force -Path $script:ProjectsDir | Out-Null
                Write-Info "Created projects directory: $($script:ProjectsDir)"
            }
        } `
        -CleanAction {
            # Never delete the user's projects directory automatically.
            Write-Warn "Clean is a no-op for Prerequisites (the projects directory is preserved)."
        }

    # 2. Visual Studio 2026 (IDE for Full, Build Tools for CI).
    # These are script-scoped so the step scriptblocks (which execute in the engine scope,
    # not in Get-Steps) can still resolve them.
    $script:VsPackage   = if ($Setup -eq 'BuildTools') { $script:VsPackageBuildTools } else { $script:VsPackageFull }
    $script:VsWorkloads = @()
    $script:VsWorkloads += if ($Setup -eq 'BuildTools') { $script:VsWorkloadNativeTools } else { $script:VsWorkloadNativeIde }
    $script:VsWorkloads += $script:VsWorkloadWinUI
    $script:VsComponents = @($script:VsComponentGit, $script:VsComponentWinSdk)

    $steps += New-Step -Name "VisualStudio" -Description "Visual Studio 2026 ($Setup) with C++/WinUI workloads" `
        -CheckIfSatisfied {
            $native = if ($Setup -eq 'BuildTools') { $script:VsWorkloadNativeTools } else { $script:VsWorkloadNativeIde }
            (Test-VsWorkload $native) -and (Test-VsWorkload $script:VsComponentWinSdk)
        } `
        -Install {
            if (-not (Test-CommandExists 'winget')) {
                throw "winget is required to install Visual Studio. Install 'App Installer' from the Microsoft Store, then re-run."
            }
            if ([string]::IsNullOrWhiteSpace($script:VsPackage)) {
                throw "Visual Studio package id is empty. This is an internal error in the script configuration."
            }
            $overrideParts = @("--quiet", "--norestart")
            foreach ($w in $script:VsWorkloads)  { $overrideParts += "--add"; $overrideParts += $w }
            foreach ($c in $script:VsComponents) { $overrideParts += "--add"; $overrideParts += $c }
            $overrideParts += "--includeRecommended"
            $override = $overrideParts -join ' '

            Write-Info "Installing $($script:VsPackage) with workloads/components: $(($script:VsWorkloads + $script:VsComponents) -join ', ')"
            Invoke-Native -FilePath "winget" -Arguments @(
                "install", "--id", $script:VsPackage, "-e",
                "--accept-package-agreements", "--accept-source-agreements",
                "--override", $override
            )
        } `
        -CleanAction {
            if (Test-CommandExists 'winget') {
                Write-Info "Uninstalling $($script:VsPackage)"
                winget uninstall --id $script:VsPackage -e --accept-source-agreements | Out-Null
            } else {
                Write-Warn "winget not available, cannot uninstall Visual Studio automatically."
            }
        }

    # 3. Clone of the desktop-kDrive repository + submodules.
    $steps += New-Step -Name "CloneRepo" -Description "Clone desktop-kDrive and init submodules" `
        -CheckIfSatisfied {
            (Test-Path (Join-Path $script:RepoDir ".git")) -and `
            (Test-Path (Join-Path $script:RepoDir "CMakeLists.txt"))
        } `
        -Install {
            if (-not (Test-Path (Join-Path $script:RepoDir ".git"))) {
                Invoke-Native -FilePath "git" -Arguments @("clone", $script:RepoUrl, $script:RepoDir)
            }
            Invoke-Native -FilePath "git" -Arguments @("submodule", "update", "--init", "--recursive") `
                -WorkingDirectory $script:RepoDir
        } `
        -CleanAction {
            if (Test-Path $script:RepoDir) {
                Remove-Item -Recurse -Force $script:RepoDir
                Write-Info "Removed $($script:RepoDir)"
            }
        }

    # 4. CPPUnit (x64 build, lib/include copied to C:\Program Files (x86)\cppunit).
    $steps += New-Step -Name "CppUnit" -Description "Build and install CPPUnit (x64)" `
        -CheckIfSatisfied {
            (Test-Path (Join-Path $script:CppUnitDir "include\cppunit")) -and `
            (Test-Path (Join-Path $script:CppUnitDir "lib"))
        } `
        -Install {
            if (-not (Test-Path (Join-Path $script:CppUnitSrcDir ".git"))) {
                # Mirror managed by freedesktop ; see Readme.md for the alternative download.
                Invoke-Native -FilePath "git" -Arguments @(
                    "clone", "git://anongit.freedesktop.org/git/libreoffice/cppunit", $script:CppUnitSrcDir
                )
            }
            $sln = Get-ChildItem -Path (Join-Path $script:CppUnitSrcDir "src") -Filter "CppUnitLibraries*.sln" -ErrorAction SilentlyContinue | Select-Object -First 1
            if (-not $sln) { throw "CPPUnit solution not found under $($script:CppUnitSrcDir)\src." }

            # Make msbuild available automatically (no need for a 'x64 Native Tools Command Prompt').
            Enter-VsDeveloperEnvironment
            if (-not (Test-CommandExists 'msbuild')) {
                throw "msbuild is still not available after loading the Visual Studio environment. Ensure the C++ workload is installed (run the 'VisualStudio' step)."
            }
            # The CPPUnit solution ships with the legacy v100 toolset; retarget it to v145 so it
            # builds with the modern Visual Studio C++ build tools installed by this script.
            Invoke-Native -FilePath "msbuild" -Arguments @($sln.FullName, "/p:Configuration=Debug",   "/p:Platform=x64", "/p:PlatformToolset=v145")
            Invoke-Native -FilePath "msbuild" -Arguments @($sln.FullName, "/p:Configuration=Release", "/p:Platform=x64", "/p:PlatformToolset=v145")

            New-Item -ItemType Directory -Force -Path $script:CppUnitDir | Out-Null
            Copy-Item -Recurse -Force (Join-Path $script:CppUnitSrcDir "lib")     $script:CppUnitDir
            Copy-Item -Recurse -Force (Join-Path $script:CppUnitSrcDir "include") $script:CppUnitDir
        } `
        -CleanAction {
            if (Test-Path $script:CppUnitDir) { Remove-Item -Recurse -Force $script:CppUnitDir; Write-Info "Removed $($script:CppUnitDir)" }
        }

    # 5. Zlib 1.2.11 (nmake build, installed to C:\Program Files (x86)\zlib-1.2.11).
    $steps += New-Step -Name "Zlib" -Description "Build and install zlib $($script:ZlibVersion)" `
        -CheckIfSatisfied {
            (Test-Path (Join-Path $script:ZlibDir "include\zlib.h")) -and `
            (Test-Path (Join-Path $script:ZlibDir "lib\zlib.lib"))
        } `
        -Install {
            # Make nmake available automatically (no need for a 'x64 Native Tools Command Prompt').
            Enter-VsDeveloperEnvironment
            if (-not (Test-CommandExists 'nmake')) {
                throw "nmake is still not available after loading the Visual Studio environment. Ensure the C++ workload is installed (run the 'VisualStudio' step)."
            }
            $archive = Join-Path $script:DownloadDir "zlib-$($script:ZlibVersion).tar.gz"
            if (-not (Test-Path $archive)) { Get-File -Url $script:ZlibUrl -Destination $archive }
            if (-not (Test-Path $script:ZlibSrcDir)) {
                Invoke-Native -FilePath "tar" -Arguments @("-xvzf", $archive, "-C", $script:ProjectsDir)
            }

            Invoke-Native -FilePath "nmake" -Arguments @("/f", "win32/Makefile.msc") -WorkingDirectory $script:ZlibSrcDir

            $incDst = Join-Path $script:ZlibDir "include"
            $libDst = Join-Path $script:ZlibDir "lib"
            $binDst = Join-Path $script:ZlibDir "bin"
            New-Item -ItemType Directory -Force -Path $incDst, $libDst, $binDst | Out-Null
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zconf.h")    $incDst
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zlib.h")     $incDst
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zdll.lib")   $libDst
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zlib.lib")   $libDst
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zlib.pdb")   $libDst -ErrorAction SilentlyContinue
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zlib1.dll")  $binDst
            Copy-Item -Force (Join-Path $script:ZlibSrcDir "zlib1.pdb")  $binDst -ErrorAction SilentlyContinue
        } `
        -CleanAction {
            if (Test-Path $script:ZlibDir) { Remove-Item -Recurse -Force $script:ZlibDir; Write-Info "Removed $($script:ZlibDir)" }
        }

    # 6. libzip v1.10.1 (depends on zlib ; install Debug + Release).
    $steps += New-Step -Name "Libzip" -Description "Build and install libzip $($script:LibzipVersion)" `
        -CheckIfSatisfied {
            (Test-Path (Join-Path $script:LibzipDir "include\zip.h")) -and `
            (Test-Path (Join-Path $script:LibzipDir "bin"))
        } `
        -Install {
            if (-not (Test-Path (Join-Path $script:ZlibDir "lib\zlib.lib"))) {
                throw "libzip requires zlib. Run the 'Zlib' step first."
            }
            # Make the MSVC compiler available to CMake automatically.
            Enter-VsDeveloperEnvironment
            if (-not (Test-Path (Join-Path $script:LibzipSrcDir ".git"))) {
                Invoke-Native -FilePath "git" -Arguments @("clone", $script:LibzipUrl, $script:LibzipSrcDir)
            }
            Invoke-Native -FilePath "git" -Arguments @("checkout", "tags/$($script:LibzipVersion)") -WorkingDirectory $script:LibzipSrcDir

            $buildDir = Join-Path $script:LibzipSrcDir "build"
            New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
            $zlibLib = Join-Path $script:ZlibDir "lib\zlib.lib"
            $zlibInc = (Join-Path $script:ZlibDir "include").Replace('\', '/')
            Invoke-Native -FilePath "cmake" -Arguments @(
                "..", "-DZLIB_LIBRARY=$zlibLib", "-DZLIB_INCLUDE_DIR:PATH=$zlibInc"
            ) -WorkingDirectory $buildDir
            Invoke-Native -FilePath "cmake" -Arguments @("--build", ".", "--target", "install", "--config", "Debug")   -WorkingDirectory $buildDir
            Invoke-Native -FilePath "cmake" -Arguments @("--build", ".", "--target", "install", "--config", "Release") -WorkingDirectory $buildDir
        } `
        -CleanAction {
            if (Test-Path $script:LibzipDir) { Remove-Item -Recurse -Force $script:LibzipDir; Write-Info "Removed $($script:LibzipDir)" }
        }

    # 7. NSIS v3.03 + plugins (LogicLib, nsProcess, UAC, x64) + PATH.
    $steps += New-Step -Name "Nsis" -Description "Install NSIS 3.03 and required plugins" `
        -CheckIfSatisfied {
            (Test-Path (Join-Path $script:NsisDir "makensis.exe")) -and (Test-InMachinePath $script:NsisDir)
        } `
        -Install {
            if (-not (Test-Path (Join-Path $script:NsisDir "makensis.exe"))) {
                $installer = Join-Path $script:DownloadDir "nsis-3.03-setup.exe"
                Get-File -Url $script:NsisUrl -Destination $installer
                Invoke-Native -FilePath $installer -Arguments @("/S")
            }
            foreach ($plugin in $script:NsisPlugins) {
                Write-Warn "Ensure the NSIS plugin '$plugin' is present under $($script:NsisDir)\Plugins (see Readme.md)."
            }
            Add-ToMachinePath $script:NsisDir
        } `
        -CleanAction {
            Remove-FromMachinePath $script:NsisDir
            if (Test-CommandExists 'winget') { winget uninstall --id "NSIS.NSIS" -e --accept-source-agreements 2>$null | Out-Null }
        }

    # 8. 7za 23.01 (extracted to C:\Program Files\7-Zip).
    $steps += New-Step -Name "SevenZip" -Description "Install 7za 23.01 extra package" `
        -CheckIfSatisfied {
            Test-Path (Join-Path $script:SevenZipDir "7za.exe")
        } `
        -Install {
            $archive = Join-Path $script:DownloadDir "7z2301-extra.7z"
            Get-File -Url $script:SevenZipUrl -Destination $archive
            $sevenZipExe = Join-Path $script:SevenZipDir "7z.exe"
            if (-not (Test-Path $sevenZipExe)) {
                throw "7-Zip must be installed first (7z.exe not found under $($script:SevenZipDir)). See Readme.md."
            }
            New-Item -ItemType Directory -Force -Path $script:SevenZipDir | Out-Null
            Invoke-Native -FilePath $sevenZipExe -Arguments @("x", $archive, "-o$($script:SevenZipDir)", "-y")
        } `
        -CleanAction {
            $target = Join-Path $script:SevenZipDir "7za.exe"
            if (Test-Path $target) { Remove-Item -Force $target; Write-Info "Removed $target" }
        }

    # 9. Icoutils 0.32.3 + PATH.
    $steps += New-Step -Name "Icoutils" -Description "Install icoutils 0.32.3" `
        -CheckIfSatisfied {
            (Test-Path (Join-Path $script:IcoutilsDir "bin")) -and (Test-InMachinePath (Join-Path $script:IcoutilsDir "bin"))
        } `
        -Install {
            $archive = Join-Path $script:DownloadDir "$($script:IcoutilsName).zip"
            if (-not (Test-Path (Join-Path $script:IcoutilsDir "bin"))) {
                Get-File -Url $script:IcoutilsUrl -Destination $archive
                Expand-Archive -Path $archive -DestinationPath $script:ProjectsDir -Force
            }
            Add-ToMachinePath (Join-Path $script:IcoutilsDir "bin")
        } `
        -CleanAction {
            Remove-FromMachinePath (Join-Path $script:IcoutilsDir "bin")
            if (Test-Path $script:IcoutilsDir) { Remove-Item -Recurse -Force $script:IcoutilsDir; Write-Info "Removed $($script:IcoutilsDir)" }
        }

    # 10. Certificates / KDC_*_AUMID : manual, interactive ; never automated (skipped on CI).
    $steps += New-Step -Name "Certificates" -Description "Signing certificates and KDC_*_AUMID env vars (manual)" -ManualOnly -BuildToolsSkip `
        -CheckIfSatisfied {
            $debug   = [Environment]::GetEnvironmentVariable('KDC_DEBUG_AUMID',   'User')
            $release = [Environment]::GetEnvironmentVariable('KDC_RELEASE_AUMID', 'User')
            if ([string]::IsNullOrEmpty($debug) -or [string]::IsNullOrEmpty($release)) {
                Write-Warn "Manual action required: install the Infomaniak certificates and set KDC_DEBUG_AUMID / KDC_RELEASE_AUMID (see 'Certificate Configuration' in Readme.md)."
                return $false
            }
            return $true
        }

    # 11. Conan : Python venv (.venv) + Conan 2 + profile detect + project dependencies.
    $steps += New-Step -Name "Conan" -Description "Python venv, Conan 2 and project dependencies (Debug)" `
        -CheckIfSatisfied {
            $venvConan = Join-Path $script:RepoDir ".venv\Scripts\conan.exe"
            $profile   = Join-Path $env:USERPROFILE ".conan2\profiles\default"
            (Test-Path $venvConan) -and (Test-Path $profile)
        } `
        -Install {
            if (-not (Test-Path (Join-Path $script:RepoDir ".git"))) {
                throw "The repository must be cloned first. Run the 'CloneRepo' step."
            }
            # Conan builds native dependencies, so the MSVC toolchain must be on PATH.
            Enter-VsDeveloperEnvironment
            $venvDir    = Join-Path $script:RepoDir ".venv"
            $venvPython = Join-Path $venvDir "Scripts\python.exe"
            $venvConan  = Join-Path $venvDir "Scripts\conan.exe"
            $activate   = Join-Path $venvDir "Scripts\Activate.ps1"

            if (-not (Test-Path $venvPython)) {
                Invoke-Native -FilePath "python" -Arguments @("-m", "venv", $venvDir)
            }
            Invoke-Native -FilePath $venvPython -Arguments @("-m", "pip", "install", "--upgrade", "pip")
            Invoke-Native -FilePath $venvPython -Arguments @("-m", "pip", "install", "conan")

            if (-not (Test-Path (Join-Path $env:USERPROFILE ".conan2\profiles\default"))) {
                Invoke-Native -FilePath $venvConan -Arguments @("profile", "detect")
            }

            # Activate the venv so build_dependencies.ps1 finds conan on PATH, then install Debug deps.
            . $activate
            Invoke-Native -FilePath "powershell" -Arguments @(
                "-NoProfile", "-File",
                (Join-Path $script:RepoDir "infomaniak-build-tools\conan\build_dependencies.ps1"),
                "Debug"
            ) -WorkingDirectory $script:RepoDir
        } `
        -CleanAction {
            $venvDir = Join-Path $script:RepoDir ".venv"
            if (Test-Path $venvDir) { Remove-Item -Recurse -Force $venvDir; Write-Info "Removed $venvDir" }
        }

    # 12. PATH update (libzip\bin, cppunit\bin, NSIS, icoutils\bin), idempotent.
    $steps += New-Step -Name "UpdatePath" -Description "Add dependency folders to the machine PATH" `
        -CheckIfSatisfied {
            $missing = $script:PathEntries | Where-Object { -not (Test-InMachinePath $_) }
            return ($missing.Count -eq 0)
        } `
        -Install {
            foreach ($entry in $script:PathEntries) { Add-ToMachinePath $entry }
        } `
        -CleanAction {
            foreach ($entry in $script:PathEntries) { Remove-FromMachinePath $entry }
        }

    return $steps
}

#################################################################################################
#                                                                                               #
#                                            ENGINE                                            #
#                                                                                               #
#################################################################################################

function Select-Steps {
    param([Parameter(Mandatory = $true)] [object[]] $Steps)

    $selected = $Steps
    if ($Only -and $Only.Count -gt 0) {
        $selected = $selected | Where-Object { $Only -contains $_.Name }
    }
    if ($Skip -and $Skip.Count -gt 0) {
        $selected = $selected | Where-Object { $Skip -notcontains $_.Name }
    }
    if ($Setup -eq 'BuildTools') {
        $selected = $selected | Where-Object { -not $_.BuildToolsSkip }
    }
    return $selected
}

function Add-Result {
    param([string] $Name, [string] $Status)
    $script:Results += [pscustomobject]@{ Step = $Name; Status = $Status }
}

function Invoke-Step {
    param([Parameter(Mandatory = $true)] [object] $Step)

    Write-Section "$($Step.Name) - $($Step.Description)"

    # Manual / interactive steps are only detected ; never automated.
    if ($Step.ManualOnly) {
        $ok = & $Step.CheckIfSatisfied
        if ($ok) { Write-Ok "OK (manual prerequisite already satisfied)"; Add-Result $Step.Name "OK" }
        else     { Write-Warn "MANUAL action required (see warning above)";  Add-Result $Step.Name "MANUAL" }
        return $true
    }

    try {
        if ($Clean) {
            Write-Info "Cleaning..."
            & $Step.Clean
            $stillSatisfied = & $Step.CheckIfSatisfied
            if ($stillSatisfied) { Write-Warn "CLEANED (some artefacts may remain)"; Add-Result $Step.Name "CLEANED" }
            else                 { Write-Ok "CLEANED"; Add-Result $Step.Name "CLEANED" }
            return $true
        }

        $satisfied = & $Step.CheckIfSatisfied
        if ($satisfied -and -not $Force) {
            Write-Ok "SKIPPED (already satisfied)"
            Add-Result $Step.Name "SKIPPED"
            return $true
        }

        Write-Info "Installing..."
        & $Step.Install

        $satisfied = & $Step.CheckIfSatisfied
        if ($satisfied) {
            Write-Ok "INSTALLED"
            Add-Result $Step.Name "INSTALLED"
            return $true
        } else {
            Write-Err "FAILED (still not satisfied after Install)"
            Add-Result $Step.Name "FAILED"
            return $false
        }
    } catch {
        Write-Err "FAILED: $($_.Exception.Message)"
        Add-Result $Step.Name "FAILED"
        return $false
    }
}

function Show-Summary {
    Write-Section "Summary"
    $script:Results | Format-Table -AutoSize | Out-String | Write-Host
}

#################################################################################################
#                                                                                               #
#                                             HELP                                             #
#                                                                                               #
#################################################################################################

function Show-Help {
    Write-Host ("
    Infomaniak kDrive - Desktop
    Copyright (C) 2023-2026 Infomaniak Network SA

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
") -ForegroundColor Cyan

    Write-Host ("
This script provisions a fresh Windows development machine for desktop-kDrive,
following infomaniak-build-tools/windows/Readme.md. Every step is idempotent and
the script can be re-run safely (resume): already satisfied steps are SKIPPED.

Parameters :
    -ProjectsDir <path>   : Root folder for sources and dependencies (default: C:\Projects).
    -Setup <Full|BuildTools>
                          : Full       => complete Visual Studio 2026 IDE (developer workstation).
                            BuildTools => Visual Studio Build Tools only, no IDE (CI).
    -Only <names...>      : Run only these steps (by name).
    -Skip <names...>      : Skip these steps (by name).
    -Clean                : Run the Clean action of each selected step instead of Install.
    -Force                : Force Install even if the step is already satisfied.
    -Help                 : Display this help and exit.

Steps (in order) :
    Prerequisites, VisualStudio, CloneRepo, CppUnit, Zlib, Libzip, Nsis, SevenZip,
    Icoutils, Certificates (manual), Conan, UpdatePath.

Examples :
    # Full developer setup under the default C:\Projects
    infomaniak-build-tools\windows\init-dev-env.ps1

    # CI setup (Build Tools only, no IDE, manual interactive steps skipped)
    infomaniak-build-tools\windows\init-dev-env.ps1 -Setup BuildTools

    # Place everything under D:\Dev
    infomaniak-build-tools\windows\init-dev-env.ps1 -ProjectsDir D:\Dev

    # Re-run only the Conan step, forcing reinstall
    infomaniak-build-tools\windows\init-dev-env.ps1 -Only Conan -Force

    # Clean only the Zlib step
    infomaniak-build-tools\windows\init-dev-env.ps1 -Clean -Only Zlib
") -ForegroundColor Yellow
}

#################################################################################################
#                                                                                               #
#                                          MAIN                                                #
#                                                                                               #
#################################################################################################

if ($Help) {
    Show-Help
    exit 0
}

# Preserve the original arguments so the elevated instance receives the same configuration.
$script:BoundArgs = @("-ProjectsDir", "`"$ProjectsDir`"", "-Setup", $Setup)
if ($Only)  { $script:BoundArgs += @("-Only",  ($Only  -join ',')) }
if ($Skip)  { $script:BoundArgs += @("-Skip",  ($Skip  -join ',')) }
if ($Clean) { $script:BoundArgs += "-Clean" }
if ($Force) { $script:BoundArgs += "-Force" }

# The README requires the setup to run elevated. Try to auto-elevate, otherwise fail clearly.
Invoke-SelfElevation

Write-Section "kDrive Windows dev environment bootstrap"
Write-Info "ProjectsDir : $ProjectsDir"
Write-Info "Setup       : $Setup"
Write-Info "Mode        : $(if ($Clean) { 'Clean' } else { 'Install' })$(if ($Force) { ' (Force)' } else { '' })"

$allSteps = Get-Steps
$steps    = Select-Steps -Steps $allSteps

if (-not $steps -or $steps.Count -eq 0) {
    Write-Warn "No step selected with the provided -Only/-Skip filters."
    exit 0
}

$hadFailure = $false
foreach ($step in $steps) {
    $ok = Invoke-Step -Step $step
    if (-not $ok) {
        $hadFailure = $true
        # Stop on the first failure: later steps usually depend on earlier ones, so
        # continuing would only produce noise. Re-run after fixing to resume.
        Write-Err "Stopping: step '$($step.Name)' failed. Remaining steps were not executed."
        break
    }
}

Show-Summary

if ($hadFailure) {
    Write-Err "A step failed. Fix the issue above and re-run (the script will resume from where it stopped)."
    exit 1
}

Write-Ok "All selected steps completed successfully."
exit 0
