$ErrorActionPreference = "Stop"

$root = $PSScriptRoot

function Get-RelativePath([string]$fromPath, [string]$toPath) {
    $fromUri = [System.Uri]::new([System.IO.Path]::GetFullPath($fromPath) + [System.IO.Path]::DirectorySeparatorChar)
    $toUri = [System.Uri]::new([System.IO.Path]::GetFullPath($toPath))
    $relativeUri = $fromUri.MakeRelativeUri($toUri)
    return [System.Uri]::UnescapeDataString($relativeUri.ToString()) -replace '/', [System.IO.Path]::DirectorySeparatorChar
}

$sourceFiles = Get-ChildItem -Path $root -Recurse -File -Include *.cs, *.xaml |
    Where-Object { $_.FullName -notmatch "\\(bin|obj)\\" -and $_.Name -ne "BindingLocalizerHelper.cs" }

$reswFiles = Get-ChildItem -Path $root -Recurse -File -Include *.resw |
    Where-Object { $_.FullName -notmatch "\\(bin|obj)\\" }

if (-not $reswFiles) {
    Write-Error "No .resw files found."
    exit 1
}

$literalPattern = 'Localizer\.Instance\.GetString\(\s*[''"](?<key>[^''"]+)[''"]'
$converterPattern = 'ConverterParameter\s*=\s*[''"][^''"]*key=(?<key>[^''";]+)'
$dynamicPattern = 'Localizer\.Instance\.GetString\(\s*(?![''"])(?<arg>[^)]*)\)'

$keys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$usages = @{}

foreach ($file in $sourceFiles) {
    $content = Get-Content -Path $file.FullName -Raw
    $relativePath = Get-RelativePath -fromPath $root -toPath $file.FullName

    foreach ($match in [regex]::Matches($content, $literalPattern)) {
        $key = $match.Groups["key"].Value
        $null = $keys.Add($key)

        if (-not $usages.ContainsKey($key)) {
            $usages[$key] = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        }

        $lineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
        $null = $usages[$key].Add("$($relativePath):$lineNumber")
    }

    foreach ($match in [regex]::Matches($content, $converterPattern)) {
        $key = $match.Groups["key"].Value
        $null = $keys.Add($key)

        if (-not $usages.ContainsKey($key)) {
            $usages[$key] = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
        }

        $lineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
        $null = $usages[$key].Add("$($relativePath):$lineNumber")
    }

    foreach ($match in [regex]::Matches($content, $dynamicPattern, [System.Text.RegularExpressions.RegexOptions]::Singleline)) {
        $lineNumber = ($content.Substring(0, $match.Index) -split "`n").Count
        $snippet = ($match.Value -replace "\s+", " ").Trim()
        Write-Warning "Cannot validate $snippet in $($relativePath):$lineNumber"
    }
}

$missingFound = $false
$missingByKey = @{}

foreach ($resw in $reswFiles) {
    $resx = [xml](Get-Content -Path $resw.FullName -Raw)
    $resourceKeys = @()

    if ($resx.root.data) {
        $resourceKeys = $resx.root.data | ForEach-Object { $_.name }
    }

    $resourceSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

    foreach ($resourceKey in $resourceKeys) {
        $null = $resourceSet.Add($resourceKey)
    }

    $language = Split-Path -Path $resw.DirectoryName -Leaf

    foreach ($key in $keys) {
        if (-not $resourceSet.Contains($key)) {
            if (-not $missingByKey.ContainsKey($key)) {
                $missingByKey[$key] = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
            }

            $null = $missingByKey[$key].Add($language)
            $missingFound = $true
        }
    }
}

if ($missingFound) {
    Write-Host "Missing keys:" 

    foreach ($key in ($missingByKey.Keys | Sort-Object)) {
        Write-Host "- $key"
        Write-Host "  usages:"

        if ($usages.ContainsKey($key)) {
            foreach ($usage in ($usages[$key] | Sort-Object)) {
                Write-Host "  - $usage"
            }
        } else {
            Write-Host "  - (no usage locations found)"
        }

        $missingLanguages = $missingByKey[$key] | Sort-Object
        Write-Host "  missing language: $($missingLanguages -join ', ')"
        Write-Host ""
    }

    exit 1
}

Write-Host "All translation keys are present in all .resw files."