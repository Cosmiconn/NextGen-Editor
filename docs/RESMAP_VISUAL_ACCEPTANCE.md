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
- `review.tsv` — selected NIF SHA-256 plus SHA-256 for all three BMP snapshots and the per-frame review metrics;
- `frame_metrics.tsv` (Python runner) — visible-pixel counts, coverage, non-black frame bounds, border-contact counts, exact changed-pixel counts between the deterministic samples and non-failing review flags;
- `runtime.log` — OpenGL vendor/renderer/version, visible-pixel counts, GL errors and warm-frame timings;
- `snapshots/<category>/...` — BMP + PPM captures;
- `VISUAL_REVIEW_CHECKLIST.md` — per-category manual review checklist;
- `index.html` — side-by-side gallery for the three fixed time samples.

The SHA-256 values are evidence identifiers, not a cross-GPU pixel-equality gate.
Driver/GPU rasterization can differ slightly; unexplained visual differences still require inspection.

The Python runner also decodes the generated 24-bit BMPs and validates the evidence itself. A category is rejected if all three deterministic samples are blank. Dynamic categories are rejected when they change **zero rendered pixels** across all three samples; this is stronger and more transparent than merely comparing file hashes. Frame-edge contact is recorded as a review flag rather than a hard failure because legitimate terrain/water/large geometry may intentionally reach the viewport boundary. Coverage, bounds and edge-contact counts remain diagnostic evidence only; no arbitrary visual-quality threshold is introduced.

## Running the full visual matrix

Use a CI artifact from the **exact branch head** being reviewed.

### Linux / headless software OpenGL

The Linux artifact contains `nif_material_inventory`, `nif_visual_matrix`, `test_nif_opengl`, `audit_resmap.py`, `verify_resmap_visual.py` and `capture_nifskope_reference.py`. The Python runner is the preferred path for automated/headless evidence because it can run the same OpenGL renderer under Xvfb with Mesa software rendering. The separate `NifSkope-reference-linux` CI artifact provides the pinned independent NifSkope 2.0.dev11 runtime used for supported Gamebryo reference captures.

```bash
python3 verify_resmap_visual.py \
  --matrix-exe ./nif_visual_matrix \
  --snapshot-exe ./test_nif_opengl \
  --strict-audit-exe ./nif_material_inventory \
  --output ./resmap-visual-matrix \
  --xvfb \
  --nifskope-reference ./NifSkope-reference-linux/run-nifskope-reference.sh \
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

The Linux CI job smoke-tests the complete runner control path — normalization, selector invocation, Xvfb/OpenGL snapshots, evidence hashing, BMP metric extraction and review/gallery generation — rather than only syntax-checking the script. CI additionally requires a non-empty `frame_metrics.tsv` with the expected metric columns and at least one visible rendered sample. The pinned NifSkope runtime is also launched under Xvfb, centered on the selected fixture NIF, captured to a non-trivial PNG, and then exercised through the integrated `verify_resmap_visual.py --nifskope-reference` path. The integrated smoke requires one NifSkope PNG for every selected matrix row while the temporary merged client tree is still alive.

### Secure CI fallback when the local executor is unavailable

The repository is public, so the raw Fiesta/ResMap corpus must **not** be committed to Git, uploaded as a public release, or attached to a normal Actions artifact.

The preferred path is `RESMAP_CORPUS_URLS`: one GitHub Actions secret containing a JSON array of exactly ten private HTTPS direct-download URLs in this fixed order: `resmap_1.zip`, `resmap_2.zip`, `resmap_3.1.zip`, `resmap_3.2.zip`, `resmap_4.zip`, `resmap_5.1.zip`, `Resmap_5.2.zip`, `Resmap_5.3.zip`, `Resmap_5.4.zip`, `Resmap_5.5.zip`. Short-lived Dropbox download URLs are suitable for this mode. The URLs are read from the secret environment value and are never printed by the workflow.

The legacy/bundle fallback remains `RESMAP_CORPUS_URL`: one private URL returning an outer ZIP bundle containing those same ten archives. Optional `RESMAP_CORPUS_SHA256` pins the outer bundle bytes.

On a manual `workflow_dispatch` (or an explicit push whose commit message contains `[resmap-full]`), the Ubuntu job:

1. detects the direct-URL or bundle mode without logging secret values;
2. downloads the ten archives to fixed local names;
3. validates every archive against the known exact byte size and runs `ZipFile.testzip()` before any renderer work;
4. extracts the corpus only once;
5. runs the **physical** strict audit on the original ten extracted roots with all discovered ResMap asset roots;
6. reuses the same extraction to build the deduplicated production-like runtime tree;
7. runs the strict 18-category NextGen OpenGL matrix plus independent NifSkope captures;
8. hard-checks the known corpus provenance (10 roots, 6,217 merged unique files, 168 identical duplicates, 3,765 physical NIFs, 3,685 logical NIF paths), `rendererGapFiles=0`, 18 review rows, one NifSkope reference per row, and zero automated frame flags;
9. uploads only `ResMap-full-reference-evidence-<SHA>` for 30 days. The raw corpus and source URLs are never re-uploaded.

Once the run completes, the evidence artifact can be downloaded independently for the final 18-category visual/reference review.

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

## Latest full-corpus evidence run

The real ten-archive ResMap matrix was executed on renderer/tooling head `66fe07f8e960eac2d29b1483a9364ef87655612d` with the Linux/Xvfb artifact from that exact head.

- normalization: 10 source roots, 6,217 merged unique files, 168 byte-identical duplicates, 0 conflicting duplicate paths;
- corpus accounting: 3,765 physical NIF entries -> 3,685 logical runtime NIF paths;
- selector: 3,685/3,685 logical NIFs loaded, 0 failed, 18/18 required categories selected, 0 missing;
- OpenGL: every selected category rendered at t=0.00/0.25/1.00 with GL error 0;
- frame metrics: no category produced three blank samples, no sample touched the viewport edge, and every review flag was empty;
- dynamic evidence: TextureTransform, FlipController, classic particles, mesh particles and world-space particles all changed actual rendered pixels across the deterministic samples.

The first full run exposed a review-selection weakness rather than a renderer-semantic gap: `IDField/EgmaDn01 2/Lava.nif` has a valid authored V-offset controller but moves only from 0 to 1 over 33.3333 seconds, so its first-second 512x512 evidence snapshots were pixel-identical. Independent real ResMap candidates proved the renderer's TextureTransform path was active. The selector now ranks candidates by normalized authored motion at the actual evidence times; without filename hardcoding it selects `KDField/KDPanMaze/Fountain.nif`, whose t=0.00/0.25/1.00 samples visibly and pixel-wise differ. No renderer material/shader semantics were changed for this correction.

This run closes the **automated full-corpus visual matrix gate**, but not the final manual/reference gate below. The tooling can now capture an independent NifSkope reference PNG for every selected row during the same merged-tree run; that capture path is CI-proven, but the real 18-category ten-archive NifSkope/reference review has not yet been accepted. NifSkope is an independent reference only for the Gamebryo semantics it actually supports. Fiesta-specific named shaders and engine behavior must use the stronger source/data evidence instead of treating a NifSkope difference as authoritative. ResMap must remain open until no unexplained renderer deviation remains.

The runner records this distinction as `ReferenceAuthority` in `review.tsv`, the checklist and the HTML gallery:

- ordinary Gamebryo categories: independent NifSkope capture plus authored NIF/material/render-state evidence;
- `PgTerrain`, `VCAlphaTextureBlender`, `AlphaTextureBlender11`: the verified Fiesta authored corpus/map contract is authoritative; NifSkope is contextual only;
- `AlphaTextureBlender`: the stock Gamebryo 2.6 shader source plus authored maps is authoritative; NifSkope is supporting evidence;
- `Glass`: the verified Gamebryo 2.6 shader/source contract plus authored shader extra-data is authoritative; NifSkope is supporting/context evidence.

This prevents the final review from accepting or rejecting a named-shader path merely because an independent general-purpose viewer looks different.

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
6. OpenGL runtime snapshots have no GL errors or obvious broken frames; no category is blank at all three deterministic samples, and dynamic categories change rendered pixels across at least one sample pair.
7. Manual/reference review has **no unexplained renderer deviation**. A green headless/CI snapshot run is evidence infrastructure, not a substitute for this reference comparison.

Only after that may `docs/UI_UPGRADE_PLAN.md` state:

> ResMap NIF Renderer abgeschlossen

and priority-2 `reschar` work may begin.
