#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$MatrixExe,

    [Parameter(Mandatory = $true)]
    [string]$SnapshotExe,

    [Parameter(Mandatory = $true)]
    [string[]]$InputPath,

    [string]$OutputPath = "resmap-visual-matrix",

    [switch]$AllowIncompleteMatrix
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

function Convert-ToSnapshotStem {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [regex]::Replace($Path, "[^A-Za-z0-9._-]", "_")
}

$matrix = Resolve-ExistingPath $MatrixExe
$snapshot = Resolve-ExistingPath $SnapshotExe
if (-not (Test-Path -LiteralPath $matrix -PathType Leaf)) {
    throw "Matrix executable is not a file: $matrix"
}
if (-not (Test-Path -LiteralPath $snapshot -PathType Leaf)) {
    throw "Snapshot executable is not a file: $snapshot"
}

$output = [System.IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $output) {
    Remove-Item -LiteralPath $output -Recurse -Force
}
New-Item -ItemType Directory -Path $output -Force | Out-Null

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("nextgen-resmap-visual-" + [guid]::NewGuid().ToString("N"))
$scanRoots = [System.Collections.Generic.List[string]]::new()
$archiveIndex = 0

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    foreach ($rawInput in $InputPath) {
        $input = Resolve-ExistingPath $rawInput

        if (Test-Path -LiteralPath $input -PathType Container) {
            Add-UniquePath -List $scanRoots -Path $input
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
    }

    if ($scanRoots.Count -eq 0) {
        throw "No ResMap roots were discovered."
    }

    $manifest = Join-Path $output "matrix.tsv"
    $matrixLog = Join-Path $output "matrix.log"
    $matrixArgs = [System.Collections.Generic.List[string]]::new()
    if (-not $AllowIncompleteMatrix) {
        $matrixArgs.Add("--strict")
    }
    $matrixArgs.Add("--manifest")
    $matrixArgs.Add($manifest)
    foreach ($root in $scanRoots) {
        $matrixArgs.Add($root)
    }

    Write-Host "Selecting ResMap visual matrix"
    Write-Host "  executable : $matrix"
    Write-Host "  scan roots : $($scanRoots.Count)"
    Write-Host "  manifest   : $manifest"

    & $matrix @matrixArgs 2>&1 | Tee-Object -FilePath $matrixLog
    $matrixExitCode = $LASTEXITCODE
    if ($matrixExitCode -ne 0) {
        throw "Visual matrix selection failed with exit code $matrixExitCode. See: $matrixLog"
    }
    if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
        throw "Visual matrix selector did not produce a manifest: $manifest"
    }

    $rows = @(Import-Csv -LiteralPath $manifest -Delimiter ([char]9))
    if ($rows.Count -eq 0) {
        throw "Visual matrix manifest is empty: $manifest"
    }

    $snapshotRoot = Join-Path $output "snapshots"
    New-Item -ItemType Directory -Path $snapshotRoot -Force | Out-Null
    $runtimeLog = Join-Path $output "runtime.log"
    if (Test-Path -LiteralPath $runtimeLog) {
        Remove-Item -LiteralPath $runtimeLog -Force
    }

    $reviewRows = [System.Collections.Generic.List[object]]::new()
    foreach ($row in $rows) {
        $category = [string]$row.category
        $root = [string]$row.root
        $relativePath = [string]$row.path
        if ([string]::IsNullOrWhiteSpace($category) -or
            [string]::IsNullOrWhiteSpace($root) -or
            [string]::IsNullOrWhiteSpace($relativePath)) {
            throw "Malformed visual matrix row in $manifest"
        }

        $categoryDir = Join-Path $snapshotRoot $category
        New-Item -ItemType Directory -Path $categoryDir -Force | Out-Null

        Write-Host ""
        Write-Host "Rendering [$category] $relativePath"
        & $snapshot $root $categoryDir $relativePath 2>&1 |
            Tee-Object -FilePath $runtimeLog -Append
        $snapshotExitCode = $LASTEXITCODE
        if ($snapshotExitCode -ne 0) {
            throw "OpenGL snapshot failed for [$category] $relativePath with exit code $snapshotExitCode. See: $runtimeLog"
        }

        $stem = Convert-ToSnapshotStem $relativePath
        $initialBmp = Join-Path $categoryDir ($stem + ".bmp")
        $animatedBmp = Join-Path $categoryDir ($stem + "__animated.bmp")
        if (-not (Test-Path -LiteralPath $initialBmp -PathType Leaf)) {
            throw "Missing initial snapshot for [$category]: $initialBmp"
        }
        if (-not (Test-Path -LiteralPath $animatedBmp -PathType Leaf)) {
            throw "Missing animated snapshot for [$category]: $animatedBmp"
        }

        $reviewRows.Add([pscustomobject]@{
            Category = $category
            Path = $relativePath
            Reason = [string]$row.reason
            Initial = "snapshots/$category/$stem.bmp"
            Animated = "snapshots/$category/$($stem)__animated.bmp"
        })
    }

    $checklist = Join-Path $output "VISUAL_REVIEW_CHECKLIST.md"
    $md = [System.Text.StringBuilder]::new()
    [void]$md.AppendLine("# ResMap visual reference matrix")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("Generated from real NIF parser properties by nif_visual_matrix and rendered through the normal NifMeshRenderer::LoadModelsForSet -> Draw runtime path.")
    [void]$md.AppendLine("")
    [void]$md.AppendLine("For every row compare: geometry, texture assignment, UVs, blend/alpha, depth, culling, material/color, environment, animation and particle position/motion where applicable.")
    [void]$md.AppendLine("")
    foreach ($item in $reviewRows) {
        [void]$md.AppendLine("## $($item.Category)")
        [void]$md.AppendLine("")
        [void]$md.AppendLine("- NIF: $($item.Path)")
        [void]$md.AppendLine("- Selection evidence: $($item.Reason)")
        [void]$md.AppendLine("- Initial snapshot: $($item.Initial)")
        [void]$md.AppendLine("- Animated snapshot: $($item.Animated)")
        [void]$md.AppendLine("- [ ] Geometry")
        [void]$md.AppendLine("- [ ] Texture assignment / UVs")
        [void]$md.AppendLine("- [ ] Alpha / blend / depth / culling")
        [void]$md.AppendLine("- [ ] Material / color / brightness")
        [void]$md.AppendLine("- [ ] Environment / shader-specific appearance")
        [void]$md.AppendLine("- [ ] Animation / particles (when applicable)")
        [void]$md.AppendLine("- [ ] No unexplained renderer deviation")
        [void]$md.AppendLine("")
    }
    [System.IO.File]::WriteAllText($checklist, $md.ToString(), [System.Text.UTF8Encoding]::new($false))

    $htmlPath = Join-Path $output "index.html"
    $html = [System.Text.StringBuilder]::new()
    [void]$html.AppendLine("<!doctype html><html><head><meta charset='utf-8'><title>ResMap visual matrix</title>")
    [void]$html.AppendLine("<style>body{font-family:Segoe UI,Arial,sans-serif;background:#111;color:#ddd;margin:24px}section{border:1px solid #333;border-radius:8px;padding:16px;margin:0 0 20px}h2{margin-top:0}.shots{display:flex;gap:16px;flex-wrap:wrap}.shot{min-width:300px;flex:1}.shot img{max-width:512px;width:100%;height:auto;background:#000;border:1px solid #444}code{color:#9de}</style></head><body>")
    [void]$html.AppendLine("<h1>ResMap visual reference matrix</h1>")
    foreach ($item in $reviewRows) {
        $cat = [System.Net.WebUtility]::HtmlEncode([string]$item.Category)
        $path = [System.Net.WebUtility]::HtmlEncode([string]$item.Path)
        $reason = [System.Net.WebUtility]::HtmlEncode([string]$item.Reason)
        $initial = ([string]$item.Initial).Replace("\\", "/")
        $animated = ([string]$item.Animated).Replace("\\", "/")
        [void]$html.AppendLine("<section><h2>$cat</h2><p><code>$path</code></p><p>$reason</p><div class='shots'>")
        [void]$html.AppendLine("<div class='shot'><h3>Initial</h3><img src='$initial'></div>")
        [void]$html.AppendLine("<div class='shot'><h3>Animated +250 ms</h3><img src='$animated'></div>")
        [void]$html.AppendLine("</div></section>")
    }
    [void]$html.AppendLine("</body></html>")
    [System.IO.File]::WriteAllText($htmlPath, $html.ToString(), [System.Text.UTF8Encoding]::new($false))

    Write-Host ""
    Write-Host "RESMAP VISUAL MATRIX GENERATED"
    Write-Host "  matrix    : $manifest"
    Write-Host "  snapshots : $snapshotRoot"
    Write-Host "  review    : $checklist"
    Write-Host "  gallery   : $htmlPath"
}
finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
