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

function Get-LogicalResMapRoots {
    param([Parameter(Mandatory = $true)][string]$Path)

    $resolved = Resolve-ExistingPath $Path
    $leaf = [System.IO.Path]::GetFileName($resolved)

    # Accept an already-normalized resmap root and the historical split names
    # resmap_3.1 / resmap_3.2 used by the supplied reference corpus.
    if ($leaf -imatch '^resmap(?:[_.-].+)?$') {
        return @($resolved)
    }

    $wrapped = @(
        Get-ChildItem -LiteralPath $resolved -Directory -ErrorAction Stop |
            Where-Object { $_.Name -imatch '^resmap(?:[_.-].+)?$' } |
            ForEach-Object { $_.FullName }
    )
    if ($wrapped.Count -gt 0) {
        return $wrapped
    }

    # Later reference archive parts contain IDField/fieldcine directly without
    # a resmap wrapper. In that layout the extraction root itself is resmap content.
    return @($resolved)
}

function Merge-ResMapRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceRoot,

        [Parameter(Mandatory = $true)]
        [string]$DestinationRoot
    )

    foreach ($file in Get-ChildItem -LiteralPath $SourceRoot -File -Recurse -ErrorAction Stop) {
        $relative = [System.IO.Path]::GetRelativePath($SourceRoot, $file.FullName)

        if ($file.Name.StartsWith('._', [System.StringComparison]::Ordinal) -or
            $relative -match '(^|[\\/])__MACOSX([\\/]|$)') {
            continue
        }

        $target = Join-Path $DestinationRoot $relative
        if (Test-Path -LiteralPath $target -PathType Leaf) {
            $existing = Get-Item -LiteralPath $target -ErrorAction Stop
            $same = $existing.Length -eq $file.Length

            if ($same) {
                $sourceHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
                $targetHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
                $same = [string]::Equals(
                    $sourceHash,
                    $targetHash,
                    [System.StringComparison]::OrdinalIgnoreCase)
            }

            if (-not $same) {
                $nl = [Environment]::NewLine
                throw ("Conflicting ResMap archive path: " + $relative + $nl +
                    "  existing: " + $target + $nl +
                    "  incoming: " + $file.FullName)
            }

            $script:identicalDuplicateFiles++
            continue
        }

        $parent = Split-Path -Parent $target
        if (-not (Test-Path -LiteralPath $parent -PathType Container)) {
            New-Item -ItemType Directory -Path $parent -Force | Out-Null
        }
        Copy-Item -LiteralPath $file.FullName -Destination $target
        $script:mergedFiles++
    }
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

$tempRoot = Join-Path (
    [System.IO.Path]::GetTempPath()) (
    "nextgen-resmap-visual-" + [guid]::NewGuid().ToString("N"))

$sourceRoots = [System.Collections.Generic.List[string]]::new()
$archiveIndex = 0
$mergedFiles = 0
$identicalDuplicateFiles = 0

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    foreach ($rawInput in $InputPath) {
        $input = Resolve-ExistingPath $rawInput

        if (Test-Path -LiteralPath $input -PathType Container) {
            foreach ($logicalRoot in @(Get-LogicalResMapRoots -Path $input)) {
                Add-UniquePath -List $sourceRoots -Path $logicalRoot
            }
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

        foreach ($logicalRoot in @(Get-LogicalResMapRoots -Path $extractRoot)) {
            Add-UniquePath -List $sourceRoots -Path $logicalRoot
        }
    }

    if ($sourceRoots.Count -eq 0) {
        throw "No ResMap roots were discovered."
    }

    # The uploaded corpus is physically split, but the real runtime sees one client tree.
    # Normalize all parts into <Client>/resmap before selecting or rendering references.
    # A repeated path is accepted only when both files are byte-identical.
    $clientRoot = Join-Path $tempRoot "client"
    $combinedResmap = Join-Path $clientRoot "resmap"
    New-Item -ItemType Directory -Path $combinedResmap -Force | Out-Null

    foreach ($sourceRoot in $sourceRoots) {
        Write-Host "Merging logical ResMap root: $sourceRoot"
        Merge-ResMapRoot -SourceRoot $sourceRoot -DestinationRoot $combinedResmap
    }

    # NifMeshRenderer normally receives <Client>/resmap/field/<Map> as mapDir.
    # This synthetic empty map directory gives DeriveClientAssetRoot the same layout.
    $runtimeMapDir = Join-Path $combinedResmap "field/__NextGenVisualMatrixRuntime__"
    New-Item -ItemType Directory -Path $runtimeMapDir -Force | Out-Null

    Write-Host "Normalized ResMap client tree"
    Write-Host "  source roots         : $($sourceRoots.Count)"
    Write-Host "  merged files         : $mergedFiles"
    Write-Host "  identical duplicates : $identicalDuplicateFiles"
    Write-Host "  client root          : $clientRoot"

    $manifest = Join-Path $output "matrix.tsv"
    $matrixLog = Join-Path $output "matrix.log"

    $matrixArgs = [System.Collections.Generic.List[string]]::new()
    if (-not $AllowIncompleteMatrix) {
        $matrixArgs.Add("--strict")
    }
    $matrixArgs.Add("--manifest")
    $matrixArgs.Add($manifest)
    $matrixArgs.Add($combinedResmap)

    Write-Host ""
    Write-Host "Selecting ResMap visual matrix"
    Write-Host "  executable : $matrix"
    Write-Host "  scan root  : $combinedResmap"
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
    $dynamicCategories = @(
        "texture_transform",
        "flip_controller",
        "particles_classic",
        "particles_mesh",
        "particles_world_space"
    )

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

        & $snapshot $root $categoryDir --runtime-map-dir $runtimeMapDir $relativePath 2>&1 |
            Tee-Object -FilePath $runtimeLog -Append

        $snapshotExitCode = $LASTEXITCODE
        if ($snapshotExitCode -ne 0) {
            throw "OpenGL snapshot failed for [$category] $relativePath with exit code $snapshotExitCode. See: $runtimeLog"
        }

        $stem = Convert-ToSnapshotStem $relativePath
        $initialBmp = Join-Path $categoryDir ($stem + ".bmp")
        $t025Bmp = Join-Path $categoryDir ($stem + "__animated.bmp")
        $t1Bmp = Join-Path $categoryDir ($stem + "__t1.bmp")

        if (-not (Test-Path -LiteralPath $initialBmp -PathType Leaf)) {
            throw "Missing t=0.00 snapshot for [$category]: $initialBmp"
        }
        if (-not (Test-Path -LiteralPath $t025Bmp -PathType Leaf)) {
            throw "Missing t=0.25 snapshot for [$category]: $t025Bmp"
        }
        if (-not (Test-Path -LiteralPath $t1Bmp -PathType Leaf)) {
            throw "Missing t=1.00 snapshot for [$category]: $t1Bmp"
        }

        $nifPath = Join-Path $root $relativePath
        if (-not (Test-Path -LiteralPath $nifPath -PathType Leaf)) {
            throw "Selected NIF disappeared before review hashing: $nifPath"
        }

        $nifHash = (Get-FileHash -LiteralPath $nifPath -Algorithm SHA256).Hash
        $t000Hash = (Get-FileHash -LiteralPath $initialBmp -Algorithm SHA256).Hash
        $t025Hash = (Get-FileHash -LiteralPath $t025Bmp -Algorithm SHA256).Hash
        $t100Hash = (Get-FileHash -LiteralPath $t1Bmp -Algorithm SHA256).Hash

        # Dynamic review categories must prove visible temporal behavior. Merely parsing an
        # authored controller/particle system is insufficient evidence if all sampled runtime
        # frames are byte-identical.
        if ($dynamicCategories -contains $category -and
            $t000Hash -eq $t025Hash -and
            $t025Hash -eq $t100Hash) {
            throw "Dynamic visual matrix category [$category] produced identical snapshots at t=0.00/0.25/1.00: $relativePath"
        }

        $reviewRows.Add([pscustomobject]@{
            Category = $category
            Path = $relativePath
            Reason = [string]$row.reason
            NifSha256 = $nifHash
            T000Sha256 = $t000Hash
            T025Sha256 = $t025Hash
            T100Sha256 = $t100Hash
            T000 = "snapshots/$category/$stem.bmp"
            T025 = "snapshots/$category/$($stem)__animated.bmp"
            T100 = "snapshots/$category/$($stem)__t1.bmp"
        })
    }

    $reviewTable = Join-Path $output "review.tsv"
    $reviewRows | Export-Csv -LiteralPath $reviewTable -Delimiter ([char]9) -NoTypeInformation -Encoding utf8NoBOM
    $checklist = Join-Path $output "VISUAL_REVIEW_CHECKLIST.md"
    $md = [System.Text.StringBuilder]::new()

    [void]$md.AppendLine("# ResMap visual reference matrix")
    [void]$md.AppendLine("")
    [void]$md.AppendLine(
        "Generated from real NIF parser properties by nif_visual_matrix and rendered through the normal NifMeshRenderer::LoadModelsForSet -> Draw runtime path.")
    [void]$md.AppendLine(
        "Split input archives are normalized into one production-like <Client>/resmap tree. Identical duplicate paths are deduplicated; conflicting duplicate paths fail the run.")
    [void]$md.AppendLine("")
    [void]$md.AppendLine(
        "For every row compare: geometry, texture assignment, UVs, blend/alpha, depth, culling, material/color, environment, animation and particle position/motion where applicable.")
    [void]$md.AppendLine("")

    foreach ($item in $reviewRows) {
        [void]$md.AppendLine("## $($item.Category)")
        [void]$md.AppendLine("")
        [void]$md.AppendLine("- NIF: $($item.Path)")
        [void]$md.AppendLine("- Selection evidence: $($item.Reason)")
        [void]$md.AppendLine("- NIF SHA-256: $($item.NifSha256)")
        [void]$md.AppendLine("- t=0.00 s: $($item.T000) (SHA-256 $($item.T000Sha256))")
        [void]$md.AppendLine("- t=0.25 s: $($item.T025) (SHA-256 $($item.T025Sha256))")
        [void]$md.AppendLine("- t=1.00 s: $($item.T100) (SHA-256 $($item.T100Sha256))")
        [void]$md.AppendLine("- [ ] Geometry")
        [void]$md.AppendLine("- [ ] Texture assignment / UVs")
        [void]$md.AppendLine("- [ ] Alpha / blend / depth / culling")
        [void]$md.AppendLine("- [ ] Material / color / brightness")
        [void]$md.AppendLine("- [ ] Environment / shader-specific appearance")
        [void]$md.AppendLine("- [ ] Animation / particles (when applicable)")
        [void]$md.AppendLine("- [ ] No unexplained renderer deviation")
        [void]$md.AppendLine("")
    }

    [System.IO.File]::WriteAllText(
        $checklist,
        $md.ToString(),
        [System.Text.UTF8Encoding]::new($false))

    $htmlPath = Join-Path $output "index.html"
    $html = [System.Text.StringBuilder]::new()

    [void]$html.AppendLine(
        "<!doctype html><html><head><meta charset='utf-8'><title>ResMap visual matrix</title>")
    [void]$html.AppendLine(
        "<style>body{font-family:Segoe UI,Arial,sans-serif;background:#111;color:#ddd;margin:24px}section{border:1px solid #333;border-radius:8px;padding:16px;margin:0 0 20px}h2{margin-top:0}.shots{display:flex;gap:16px;flex-wrap:wrap}.shot{min-width:300px;flex:1}.shot img{max-width:512px;width:100%;height:auto;background:#000;border:1px solid #444}code{color:#9de}</style></head><body>")
    [void]$html.AppendLine("<h1>ResMap visual reference matrix</h1>")

    foreach ($item in $reviewRows) {
        $cat = [System.Net.WebUtility]::HtmlEncode([string]$item.Category)
        $path = [System.Net.WebUtility]::HtmlEncode([string]$item.Path)
        $reason = [System.Net.WebUtility]::HtmlEncode([string]$item.Reason)
        $initial = ([string]$item.T000).Replace("\\", "/")
        $t025 = ([string]$item.T025).Replace("\\", "/")
        $t1 = ([string]$item.T100).Replace("\\", "/")
        $nifHash = [System.Net.WebUtility]::HtmlEncode([string]$item.NifSha256)

        [void]$html.AppendLine(
            "<section><h2>$cat</h2><p><code>$path</code></p><p>$reason</p><p><small>NIF SHA-256: $nifHash</small></p><div class='shots'>")
        [void]$html.AppendLine(
            "<div class='shot'><h3>t = 0.00 s</h3><img src='$initial'></div>")
        [void]$html.AppendLine(
            "<div class='shot'><h3>t = 0.25 s</h3><img src='$t025'></div>")
        [void]$html.AppendLine(
            "<div class='shot'><h3>t = 1.00 s</h3><img src='$t1'></div>")
        [void]$html.AppendLine("</div></section>")
    }

    [void]$html.AppendLine("</body></html>")
    [System.IO.File]::WriteAllText(
        $htmlPath,
        $html.ToString(),
        [System.Text.UTF8Encoding]::new($false))

    Write-Host ""
    Write-Host "RESMAP VISUAL MATRIX GENERATED"
    Write-Host "  matrix    : $manifest"
    Write-Host "  evidence  : $reviewTable"
    Write-Host "  snapshots : $snapshotRoot"
    Write-Host "  review    : $checklist"
    Write-Host "  gallery   : $htmlPath"
}
finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
