#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$AuditExe,

    [Parameter(Mandatory = $true)]
    [string[]]$InputPath,

    [string]$ReportPath = "resmap-strict-audit.txt",

    [switch]$DeferPriority2CharacterShaders
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Resolve-Path -LiteralPath $Path -ErrorAction Stop).Path
}

function Add-UniquePath {
    param(
        [Parameter(Mandatory = $true)]
        [AllowEmptyCollection()]
        [System.Collections.Generic.List[string]]$List,
        [Parameter(Mandatory = $true)]
        [string]$Path
    )
    foreach ($existing in $List) {
        if ([string]::Equals($existing, $Path, [System.StringComparison]::OrdinalIgnoreCase)) {
            return
        }
    }
    $List.Add($Path)
}

$audit = Resolve-ExistingPath $AuditExe
if (-not (Test-Path -LiteralPath $audit -PathType Leaf)) {
    throw "Audit executable is not a file: $audit"
}

$report = [System.IO.Path]::GetFullPath($ReportPath)
$reportDir = Split-Path -Parent $report
if ($reportDir -and -not (Test-Path -LiteralPath $reportDir -PathType Container)) {
    New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("nextgen-resmap-audit-" + [guid]::NewGuid().ToString("N"))
$scanRoots = [System.Collections.Generic.List[string]]::new()
$assetRoots = [System.Collections.Generic.List[string]]::new()
$archiveIndex = 0

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    foreach ($rawInput in $InputPath) {
        $input = Resolve-ExistingPath $rawInput

        if (Test-Path -LiteralPath $input -PathType Container) {
            Add-UniquePath -List $scanRoots -Path $input
            Add-UniquePath -List $assetRoots -Path $input

            Get-ChildItem -LiteralPath $input -Directory -Recurse -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -ieq "resmap" } |
                ForEach-Object { Add-UniquePath -List $assetRoots -Path $_.FullName }
            continue
        }

        if (-not (Test-Path -LiteralPath $input -PathType Leaf)) {
            throw "Input is neither a directory nor a file: $input"
        }
        if ([System.IO.Path]::GetExtension($input) -ine ".zip") {
            throw "Only directories and .zip archives are supported: $input"
        }

        $archiveIndex++
        $extractRoot = Join-Path $tempRoot ("archive-{0:D2}" -f $archiveIndex)
        New-Item -ItemType Directory -Path $extractRoot -Force | Out-Null
        Write-Host "Extracting $input -> $extractRoot"
        Expand-Archive -LiteralPath $input -DestinationPath $extractRoot -Force

        Add-UniquePath -List $scanRoots -Path $extractRoot
        Add-UniquePath -List $assetRoots -Path $extractRoot

        Get-ChildItem -LiteralPath $extractRoot -Directory -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -ieq "resmap" } |
            ForEach-Object { Add-UniquePath -List $assetRoots -Path $_.FullName }
    }

    if ($scanRoots.Count -eq 0) {
        throw "No ResMap roots were discovered."
    }

    $arguments = [System.Collections.Generic.List[string]]::new()
    $arguments.Add("--strict-renderer")
    if ($DeferPriority2CharacterShaders) {
        $arguments.Add("--defer-priority2-character-shaders")
    }
    foreach ($root in $assetRoots) {
        $arguments.Add("--asset-root")
        $arguments.Add($root)
    }
    foreach ($root in $scanRoots) {
        $arguments.Add($root)
    }

    Write-Host "Strict ResMap audit"
    Write-Host "  executable : $audit"
    Write-Host "  scan roots : $($scanRoots.Count)"
    Write-Host "  asset roots: $($assetRoots.Count)"
    Write-Host "  report     : $report"

    & $audit @arguments 2>&1 | Tee-Object -FilePath $report
    $nativeExitCode = $LASTEXITCODE

    $summary = Get-Content -LiteralPath $report |
        Where-Object { $_.StartsWith("SUMMARY") } |
        Select-Object -Last 1

    if (-not $summary) {
        throw "Audit did not emit a SUMMARY line. See: $report"
    }
    if ($nativeExitCode -ne 0) {
        throw "Strict ResMap audit failed with exit code $nativeExitCode. See: $report"
    }
    if ($summary -notmatch "(^|\s)rendererGapFiles=0($|\s)") {
        throw "Strict ResMap audit did not finish at rendererGapFiles=0. See: $report"
    }

    Write-Host ""
    Write-Host "STRICT RESMAP AUDIT PASSED"
    Write-Host $summary
}
finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
