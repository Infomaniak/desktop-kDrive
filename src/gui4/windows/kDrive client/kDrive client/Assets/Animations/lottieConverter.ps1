param(
    [Parameter(Mandatory = $true)]
    [string]$Directory
)

# Ensure the directory exists
if (-not (Test-Path $Directory)) {
    Write-Error "Directory '$Directory' does not exist."
    exit 1
}

# Get all .lottie files in the directory
Get-ChildItem -Path $Directory -Filter *.lottie | ForEach-Object {
    $lottieFile = $_.FullName
    $baseName = $_.BaseName
    $tempZip = Join-Path $Directory ($baseName + ".zip")
    $tempPath = Join-Path -Path ([System.IO.Path]::GetTempPath()) -ChildPath ([guid]::NewGuid().ToString())

    try {
        # Rename .lottie to .zip
        Rename-Item -Path $lottieFile -NewName ($baseName + ".zip")
        
        # Create temp folder
        New-Item -ItemType Directory -Path $tempPath | Out-Null

        # Unzip the renamed file
        Expand-Archive -Path $tempZip -DestinationPath $tempPath -Force

        # Find the first file inside the 'animations' folder
        $animationsFolder = Join-Path $tempPath "animations"
        if (-not (Test-Path $animationsFolder)) {
            Write-Warning "No 'animations' folder found in $tempZip"
            return
        }

        $animationFile = Get-ChildItem -Path $animationsFolder -File | Select-Object -First 1
        if (-not $animationFile) {
            Write-Warning "No animation file found in $animationsFolder"
            return
        }

        # Move and rename it to the original directory with .json extension
        $destinationFile = Join-Path $Directory ($baseName + ".json")
        Move-Item -Path $animationFile.FullName -Destination $destinationFile -Force

        Write-Host "Extracted animation from '$($_.Name)' to '$destinationFile'"
    }
    finally {
        # Clean up: remove temp zip and temp folder
        if (Test-Path $tempZip) {
            Remove-Item -Path $tempZip -Force
        }
        if (Test-Path $tempPath) {
            Remove-Item -Path $tempPath -Recurse -Force
        }
    }
}
