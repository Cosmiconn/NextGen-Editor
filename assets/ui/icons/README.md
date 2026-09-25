# NextGen UI icon assets

This directory is populated only from the two approved user-supplied icon packs.

## Source roles

- `NextGen_Icons_True_Vector_Set_With_Sizes.zip`
  - true SVG path masters;
  - curated PNG exports at 16/24/32/48/64/128 px.
- `NextGen_Icons_Complete_PNG_SVG.zip`
  - high-resolution PNG appearance reference;
  - its SVG files embed PNGs and are not used as vector masters.

Run:

```bash
python tools/ui/import_icon_pack.py \
  --vector-pack /path/NextGen_Icons_True_Vector_Set_With_Sizes.zip \
  --raster-pack /path/NextGen_Icons_Complete_PNG_SVG.zip \
  --repo-root . --clean
```

The importer refuses the vector pack if any SVG contains an embedded `<image>`,
`data:image` or Base64 raster payload, and it requires the two manifests to agree
on all 54 logical icons.

Runtime policy:
- 16 px: inline/outliner;
- 24 px: primary toolbar;
- 32 px: module launchers;
- 48/64/128: larger cards/branding where required.

Do not hand-edit generated raster exports. Update the approved source packs and
rerun the importer instead.
