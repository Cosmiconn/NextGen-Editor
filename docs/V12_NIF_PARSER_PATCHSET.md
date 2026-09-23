# v12 NIF parser patchset reference

Status: reference patchset prepared while the local build/container runtime is unavailable.
Target baseline: NextGen-Editor v11 (texture animation + palette).
Reference: niftools/niflib at e29166183e23c3477fb7e08d4523dd111440947e and NifSkope behavior.

## 1. machine.nif / NiPSysData alignment fix

Root cause observed in the Fiesta client corpus:
- the later NiPixelData failure is a downstream symptom;
- the parser has already become misaligned in the NiGeometryData -> NiParticlesData -> NiRotatingParticlesData -> NiPSysData chain;
- for the affected Fiesta version, NiGeometryData contains the versioned leading unknown uint before the vertex-count fields;
- after correcting that inheritance chain, both tested machine.nif variants parsed fully.

Do not add NiPixelData recovery heuristics for this case. Fix the base-data layout first.

## 2. NiMeshPSysData variable trailer

After the NiPSysData base, for version >= 0x0A020000 read:
- uint32 unknownInt2
- uint8 unknownByte3
- uint32 numUnknownInts1
- numUnknownInts1 * uint32
Then always read:
- int32/reference unknownNode

The old fixed 17-byte trailer is wrong. The correct variable payload is 9 + 4*N + 4 bytes after the NiPSysData base.

## 3. NiPSysModifier base

Read:
- NiObject base
- string name
- uint32 order
- int32/reference target
- uint8/bool active

Derived particle modifiers must start from this exact base.

## 4. NiPSysDragModifier

After NiPSysModifier:
- int32/reference parent
- Vector3 dragAxis
- float percentage
- float range
- float rangeFalloff

This is structural parsing only; the editor does not need to simulate the force for mesh rendering.

## 5. NiPSysColliderManager

After NiPSysModifier:
- int32/reference collider

## 6. NiPSysCollider base

Read:
- NiObject base
- float bounce
- bool spawnOnCollide
- bool dieOnCollide
- int32/reference spawnModifier
- int32/reference parent
- int32/reference nextCollider
- int32/reference colliderObject

## 7. NiPSysPlanarCollider

After NiPSysCollider:
- float width
- float height
- Vector3 xAxis
- Vector3 yAxis

## 8. NiPSysModifierActiveCtlr

Inheritance:
NiPSysModifierActiveCtlr -> NiPSysModifierBoolCtlr -> NiPSysModifierCtlr -> NiSingleInterpController

NiPSysModifierCtlr adds:
- string modifierName

NiSingleInterpController adds an interpolator reference for version >= 0x0A020000.

NiPSysModifierActiveCtlr adds a data reference only for version <= 0x0A010000.

For Fiesta NIF 0x14000004, do not read that legacy data reference.

## 9. NiPortal

After NiAVObject:
- uint16 unknownFlags
- int16 unknownShort2
- uint16 numVertices
- numVertices * Vector3
- int32/reference target

Use bounded counts before resizing/skipping.

## 10. NiRoom

After NiNode:
- int32 numWalls
- numWalls * Plane
- int32 numInPortals
- numInPortals * references
- int32 numPortals2
- numPortals2 * references
- int32 numItems
- numItems * references

Plane is four floats in the NIF data model.

## 11. NiRoomGroup

After NiNode:
- int32/reference shellLink
- int32 numRooms
- numRooms * references

## 12. NiDynamicEffect

After NiAVObject:
- if version >= 0x0A01006A: bool switchState
- if version <= 0x04000002: uint32 numAffectedNodeListPointers followed by that many uint32 legacy pointers
- if version >= 0x0A010000: uint32 numAffectedNodes followed by that many references

For Fiesta 0x14000004 use switchState + affected-node reference array.

## 13. NiTextureEffect

After NiDynamicEffect:
- Matrix33 modelProjectionMatrix
- Vector3 modelProjectionTransform
- TexFilterMode
- TexClampMode
- if version >= 0x14060000: int16 unknown
- EffectType
- CoordGenType
- if version <= 0x03010000: image reference
- if version >= 0x04000000: sourceTexture reference
- uint8 clippingPlane
- Vector3 unknownVector
- float unknownFloat
- if version <= 0x0A020000: int16 ps2L, int16 ps2K
- if version <= 0x0401000C: uint16 unknownShort

For Fiesta 0x14000004, do not consume the >=0x14060000 short or the old PS2/legacy tail fields.

## 14. NiMorphData

Read:
- NiObject base
- uint32 numMorphs
- uint32 numVertices
- uint8 relativeTargets

For each morph:
- if version >= 0x0A01006A: string frameName
- if version <= 0x0A010000: uint32 numKeys, KeyType interpolation, then numKeys float keys in that interpolation format
- if 0x0A01006A <= version <= 0x0A020000: uint32 unknownInt
- if 0x14000004 <= version <= 0x14000005 and userVersion == 0: uint32 unknownInt
- numVertices * Vector3 morph vectors

Critical: this block is variable-sized. Never skip it with a fixed byte count.

## 15. NiGeomMorpherController

Base: NiInterpController

Then:
- if version >= 0x0A000102: uint16 extraFlags
- if version == 0x0A01006A: uint8 unknown2
- int32/reference NiMorphData
- uint8 alwaysUpdate
- if version >= 0x0A01006A: uint32 numInterpolators
- if 0x0A01006A <= version <= 0x14000005: numInterpolators references
- if version >= 0x14010003: numInterpolators entries of { reference interpolator, float weight }
- if 0x14000004 <= version <= 0x14000005 and userVersion >= 10: uint32 count + count * uint32 unknownInts

The Fiesta 0x14000004 files may therefore carry both the interpolator-reference array and weighted interpolator entries depending on userVersion. Respect the exact conditions.

## 16. Safe parser rules for all newly supported blocks

- Validate every variable count against remaining bytes and a sane hard limit before allocation.
- Prefer structural parsing/skipping over scanning for the next block name.
- Do not hide alignment bugs with recovery unless a format variant is genuinely ambiguous.
- If a derived block is only needed for alignment, parse its base chain exactly and discard semantic values.
- Keep recovery as a fallback, not the normal path.

## 17. Mass-test acceptance criteria

When the local runtime is available again:
1. rebuild mapeditor_core from a clean build directory;
2. run all CTest tests; expected baseline from v11 is 10/10;
3. specifically parse both machine.nif variants and assert non-zero mesh/triangle counts;
4. scan every readable NIF from Client, resmap, resmap__2_, resmap__4_, resitem, reseffect, ressystem, resmenu and fixtures;
5. run a fast structural/default pass first;
6. retry only failures with recovery;
7. hard timeout each recovery file independently;
8. save TSV/CSV columns: archive,path,status,parts,vertices,triangles,embedded_textures,skinned,error,duration_ms;
9. report unreadable archives separately from NIF parser failures.

Known archive issue from the supplied corpus: resmap__3_.zip has no usable ZIP central directory and must not be counted as a NIF parse failure.
