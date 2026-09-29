# ResMap Visual Acceptance

## Status

The automated ResMap renderer gate is already at zero known gaps, but **ResMap is not yet declared finished**.
The final open gate is the representative visual reference review described here.

Do not start the priority-2 `reschar` / NPC / mob shader pass until this visual review has no unexplained renderer deviation.

## Reference matrix

`nif_visual_matrix` selects real Fiesta NIFs from the supplied ResMap corpus using parsed renderer semantics rather than a hand-maintained filename list.
The strict matrix currently requires all of these categories:

1. `vegetation`
2. `building_static`
3. `alpha_cutout`
4. `environment_water`
5. `glow_emissive`
6. `bump`
7. `lod`
8. `billboard`
9. `texture_transform`
10. `flip_controller`
11. `particles_classic`
12. `particles_mesh`
13. `particles_world_space`
14. `pgterrain`
15. `vc_alpha_texture_blender`
16. `alpha_texture_blender`
17. `alpha_texture_blender11`
18. `glass`

Selection deliberately prefers inspectable representatives instead of the largest stress cases.
Full-corpus stress/coverage remains the job of `nif_material_inventory --strict-renderer`.

## Supplied ResMap corpus

The known visual/full-audit input set is:

- `resmap_1.zip`
- `resmap_2.zip`
- `resmap_3.1.zip`
- `resmap_3.2.zip`
- `resmap_4.zip`
- `resmap_5.1.zip`
- `Resmap_5.2.zip`
- `Resmap_5.3.zip`
- `Resmap_5.4.zip`
- `Resmap_5.5.zip`

`verify_resmap_visual.ps1` and the cross-platform `verify_resmap_visual.py` normalize the historical archive layouts into one production-like `<Client>/resmap` tree before selection/rendering.
Both accept wrapper directories such as `resmap`, `resmap_3.1`, `resmap_3.2` and later parts whose ResMap contents are at archive root.

The supplied ten-part corpus contains **3,765 physical NIF entries** when the archive roots are audited separately. After byte-identical logical paths are deduplicated into the production-like runtime tree, there are **3,685 logical NIF paths**. These numbers measure different things and are both intentional: the strict full-corpus gate preserves every physical corpus entry, while the visual runner mirrors the single logical client tree seen by the editor.

macOS metadata (`._*`, `__MACOSX`) is excluded.
If the same logical path appears multiple times, byte-identical files are deduplicated.
A conflicting duplicate path is a hard failure.

## Runtime path exercised

The snapshot executable is `test_nif_opengl`.
For every selected NIF it:

- creates a real OpenGL 3.3 core context;
- initializes `NifMeshRenderer`;
- loads the selected model through the normal `LoadModelsForSet` runtime path;
- uses the production-like client root so normal external texture resolution is exercised;
- therefore also exercises the real sibling-embedded Fiesta fallback path;
- draws through `NifMeshRenderer::Draw`;
- checks `glGetError()`;
- clears color, depth and stencil between samples;
- checks visible pixels;
- measures warm-frame time;
- performs a hidden-object negative control.

The normal editor still uses `steady_clock`.
Only visual-regression tooling pins animation time, so snapshots are not dependent on CI/host speed.

Three deterministic samples are produced:

- `t = 0.00 s`
- `t = 0.25 s`
- `t = 1.00 s`

This gives texture controllers and particle systems more than one authored runtime state without changing their runtime semantics.

## Evidence produced

The visual runner writes:

- `provenance.json` (Python runner) — source-root, merge, duplicate and physical/logical NIF counts;
- `matrix.tsv` — selected category/NIF/evidence;
- `review.tsv` — selected NIF SHA-256 plus SHA-256 for all three BMP snapshots;
- `runtime.log` — OpenGL vendor/renderer/version, visible-pixel counts, GL errors and warm-frame timings;
- `snapshots/<category>/...` — BMP + PPM captures;
- `VISUAL_REVIEW_CHECKLIST.md` — per-category manual review checklist;
- `index.html` — side-by-side gallery for the three fixed time samples.

The SHA-256 values are evidence identifiers, not a cross-GPU pixel-equality gate.
Driver/GPU rasterization can differ slightly; unexplained visual differences still require inspection.

## Running the full visual matrix

Use a CI artifact from the **exact branch head** being reviewed.

### Linux / headless software OpenGL

The Linux artifact contains `nif_visual_matrix`, `test_nif_opengl` and `verify_resmap_visual.py`. The Python runner is the preferred path for automated/headless evidence because it can run the same OpenGL renderer under Xvfb with Mesa software rendering.

```bash
python3 verify_resmap_visual.py \
  --matrix-exe ./nif_visual_matrix \
  --snapshot-exe ./test_nif_opengl \
  --output ./resmap-visual-matrix \
  --xvfb \
  resmap_1.zip \
  resmap_2.zip \
  resmap_3.1.zip \
  resmap_3.2.zip \
  resmap_4.zip \
  resmap_5.1.zip \
  Resmap_5.2.zip \
  Resmap_5.3.zip \
  Resmap_5.4.zip \
  Resmap_5.5.zip
```

The Linux CI job smoke-tests the complete runner control path — normalization, selector invocation, Xvfb/OpenGL snapshots, evidence hashing and review/gallery generation — rather than only syntax-checking the script.

### Windows

The Windows artifact contains `nif_visual_matrix.exe`, `test_nif_opengl.exe` and `verify_resmap_visual.ps1`.

```powershell
./verify_resmap_visual.ps1 `
  -MatrixExe ./nif_visual_matrix.exe `
  -SnapshotExe ./test_nif_opengl.exe `
  -InputPath @(
    'resmap_1.zip',
    'resmap_2.zip',
    'resmap_3.1.zip',
    'resmap_3.2.zip',
    'resmap_4.zip',
    'resmap_5.1.zip',
    'Resmap_5.2.zip',
    'Resmap_5.3.zip',
    'Resmap_5.4.zip',
    'Resmap_5.5.zip'
  ) `
  -OutputPath ./resmap-visual-matrix
```

Do not use `--allow-incomplete-matrix` / `-AllowIncompleteMatrix` for final acceptance.

## What must be reviewed

For every category/NIF, compare the snapshot/output against a trustworthy Fiesta/Gamebryo visual reference and check:

- geometry and transforms;
- texture assignment;
- UV selection and transforms;
- alpha blend/test/cutout;
- depth behavior;
- culling / face direction;
- vertex/material color;
- brightness, specular and emissive appearance;
- environment/sphere/cube-map behavior;
- named-shader-specific appearance;
- LOD/billboard behavior;
- texture animation / flipbook state;
- particle position, orientation, scale, color and movement.

A snapshot merely showing pixels is not sufficient.

## Final ResMap acceptance rule

ResMap can be documented as complete only when all of the following hold on the reviewed head:

1. Full-corpus strict audit still ends with `rendererGapFiles=0`.
2. Parsing has zero failed/recovery/partial files.
3. Embedded/external texture resolution and decode remain clean.
4. No unexplained UV / ShaderTexDesc / ApplyMode / TextureEffect / render-state gaps remain.
5. The strict 18-category visual matrix is complete.
6. OpenGL runtime snapshots have no GL errors or obvious broken frames; dynamic categories visibly differ across at least one of the deterministic evidence samples.
7. Manual/reference review has **no unexplained renderer deviation**. A green headless/CI snapshot run is evidence infrastructure, not a substitute for this reference comparison.

Only after that may `docs/UI_UPGRADE_PLAN.md` state:

> ResMap NIF Renderer abgeschlossen

and priority-2 `reschar` work may begin.
