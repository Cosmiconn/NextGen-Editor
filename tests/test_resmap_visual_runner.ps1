#requires -Version 7.0

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$runner = Join-Path $repoRoot "tools/verify_resmap_visual.ps1"
$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) (
    "nextgen-resmap-visual-smoke-" + [guid]::NewGuid().ToString("N"))

function Assert-True {
    param(
        [Parameter(Mandatory = $true)]
        [bool]$Condition,
        [Parameter(Mandatory = $true)]
        [string]$Message
    )
    if (-not $Condition) {
        throw $Message
    }
}

try {
    $inputA = Join-Path $tempRoot "part-a"
    $inputB = Join-Path $tempRoot "part-b"
    $conflictInput = Join-Path $tempRoot "part-conflict"
    $output = Join-Path $tempRoot "output"
    $conflictOutput = Join-Path $tempRoot "conflict-output"
    $matrixStub = Join-Path $tempRoot "matrix-stub.ps1"
    $snapshotStub = Join-Path $tempRoot "snapshot-stub.ps1"

    New-Item -ItemType Directory -Path (Join-Path $inputA "resmap/field") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $inputB "resmap_3.1/field") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $conflictInput "resmap/field") -Force | Out-Null

    [System.IO.File]::WriteAllText(
        (Join-Path $inputA "resmap/field/demo.nif"),
        "same-fixture",
        [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText(
        (Join-Path $inputB "resmap_3.1/field/demo.nif"),
        "same-fixture",
        [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText(
        (Join-Path $inputB "resmap_3.1/field/extra.nif"),
        "extra-fixture",
        [System.Text.UTF8Encoding]::new($false))
    [System.IO.File]::WriteAllText(
        (Join-Path $conflictInput "resmap/field/demo.nif"),
        "conflicting-fixture",
        [System.Text.UTF8Encoding]::new($false))

    @'
$manifest = $null
for ($i = 0; $i -lt $args.Count; ++$i) {
    if ($args[$i] -eq "--manifest" -and $i + 1 -lt $args.Count) {
        $manifest = [string]$args[$i + 1]
        break
    }
}
if ([string]::IsNullOrWhiteSpace($manifest)) {
    throw "matrix stub did not receive --manifest"
}
$root = [string]$args[$args.Count - 1]
$tab = [char]9
$lines = @(
    ("category" + $tab + "root" + $tab + "path" + $tab + "reason"),
    ("visual_smoke" + $tab + $root + $tab + "field/demo.nif" + $tab + "stub-selection")
)
[System.IO.File]::WriteAllLines(
    $manifest,
    $lines,
    [System.Text.UTF8Encoding]::new($false))
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $matrixStub -Encoding utf8NoBOM

    @'
if ($args.Count -lt 3) {
    throw "snapshot stub received too few arguments"
}
$output = [string]$args[1]
$name = [string]$args[$args.Count - 1]
$stem = [regex]::Replace($name, "[^A-Za-z0-9._-]", "_")
New-Item -ItemType Directory -Path $output -Force | Out-Null
[System.IO.File]::WriteAllBytes(
    (Join-Path $output ($stem + ".bmp")),
    [byte[]](0x42, 0x4d))
[System.IO.File]::WriteAllBytes(
    (Join-Path $output ($stem + "__animated.bmp")),
    [byte[]](0x42, 0x4d))
$global:LASTEXITCODE = 0
'@ | Set-Content -LiteralPath $snapshotStub -Encoding utf8NoBOM

    $runnerArgs = @{
        MatrixExe = $matrixStub
        SnapshotExe = $snapshotStub
        InputPath = @($inputA, $inputB)
        OutputPath = $output
        AllowIncompleteMatrix = $true
    }
    $runOutput = & $runner @runnerArgs 2>&1 | Out-String

    Assert-True ($runOutput -match "identical duplicates\s*:\s*1") "Visual runner did not report the expected identical duplicate."

    $manifest = Join-Path $output "matrix.tsv"
    $checklist = Join-Path $output "VISUAL_REVIEW_CHECKLIST.md"
    $gallery = Join-Path $output "index.html"
    $initialSnapshot = Join-Path $output "snapshots/visual_smoke/field_demo.nif.bmp"
    $animatedSnapshot = Join-Path $output "snapshots/visual_smoke/field_demo.nif__animated.bmp"

    foreach ($required in @(
        $manifest,
        $checklist,
        $gallery,
        $initialSnapshot,
        $animatedSnapshot)) {
        Assert-True (Test-Path -LiteralPath $required -PathType Leaf) "Visual runner did not create expected output: $required"
    }

    $rows = @(Import-Csv -LiteralPath $manifest -Delimiter ([char]9))
    Assert-True ($rows.Count -eq 1) "Visual runner manifest row count changed."
    Assert-True ([string]$rows[0].category -eq "visual_smoke") "Visual runner manifest category changed."

    $checklistText = Get-Content -LiteralPath $checklist -Raw
    Assert-True ($checklistText -match "visual_smoke") "Visual review checklist omitted the selected category."
    Assert-True ($checklistText -match "field/demo\.nif") "Visual review checklist omitted the selected NIF."

    $conflictCaught = $false
    try {
        $conflictArgs = @{
            MatrixExe = $matrixStub
            SnapshotExe = $snapshotStub
            InputPath = @($inputA, $conflictInput)
            OutputPath = $conflictOutput
            AllowIncompleteMatrix = $true
        }
        & $runner @conflictArgs 2>&1 | Out-Null
    }
    catch {
        $conflictCaught = $_.Exception.Message -match "Conflicting ResMap archive path"
    }

    Assert-True $conflictCaught "Visual runner did not reject conflicting duplicate archive paths."

    Write-Host "ResMap visual runner smoke test passed."
}
finally {
    if (Test-Path -LiteralPath $tempRoot -PathType Container) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
