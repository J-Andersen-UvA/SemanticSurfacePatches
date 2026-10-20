# Semantic Surface Patches

This Unreal plugin owns reusable semantic surface-patch data and authoring tools. A patch identifies a meaningful region on one actor without depending on animation correction or collision solving.

## Modules

- `SemanticSurfacePatches`: the `SurfacePatches` actor component, visible anchor components, runtime queries, and legacy data assets.
- `SemanticSurfacePatchesEditor`: JSON preset loading, copy-from-actor, and legacy import.

## Component workflow

1. Add a `SurfacePatches` component to an actor Blueprint.
2. Select the component and click `Load Preset...`.
3. Select `Resources/Presets/humanoidSignLanguageFace.json`.
4. Select the actor's skeletal mesh component and enter the parent bone.
5. Compile the Blueprint if Unreal has not already refreshed it.
6. Select an individual `SurfacePatch_*` component in the component tree and move or rotate its yellow arrow in the Blueprint viewport.
7. Select `SurfacePatches` and click `Build Surface Patches`.
8. Select an individual anchor to inspect its translucent triangle patch.

The mesh is selected from the Blueprint's actual skeletal-mesh components. Its component name is shown as read-only metadata; it is changed by loading or copying patches rather than typed manually. Each anchor is a real scene component parented to that skeletal-mesh component and attached to the selected bone. Loading a preset replaces existing surface patch anchor components instead of appending duplicates.

The component exposes a read-only `Surface Patches` array pointing to the generated anchor components. The generated component variables use the `Surface Patches` Blueprint category so they remain grouped in the My Blueprint panel.

`Build Surface Patches` reads LOD 0 from the selected skeletal mesh. For each enabled anchor it casts inward along the anchor normal and selects the first surface hit, using the nearest triangle as a fallback. It then grows across geometrically connected triangles within the configured radius. The normal and edge-normal thresholds prevent the patch from crossing sharp folds or jumping to disconnected teeth, tongue, hair, or clothing geometry. The resulting triangles are stored in anchor-local space and are available to runtime consumers. Rebuilding replaces the previous triangles.

Patch previews use a translucent yellow procedural mesh slightly above the surface. Only the selected anchor's patch is shown. Radius and filtering settings are editable on each anchor under `Surface Patch`.

Each anchor also provides `Rebuild This Patch` and `Rotate Normal Outwards` buttons for editing one patch without processing all anchors.

Rebuilding clears and replaces each enabled anchor's stored triangles using its current transform and settings. It never appends duplicate triangles.

`Mirror Right to Left` prompts for the reference-mesh axis to negate, reflects right-side anchor locations and orientations across that plane, converts them back into the destination bone space, copies their patch settings to matching left-side anchors, and clears stale left-side triangles. `Rotate Normals Outwards` aligns every anchor to the nearest outward LOD 0 surface normal and clears stale triangles. Run `Build Surface Patches` afterward.

The included preset creates fourteen anchors: forehead; left and right temple, cheek, lower cheek, ear, and zygoma; nose; mouth; and chin. Its positions are an approximate Character Creator starting layout in parent-bone local centimetres.

`Copy Patches From Actor Blueprint...` copies anchor components from another actor Blueprint asset. Anchor transforms are converted between source and destination reference-bone orientations. If a copied bone does not exist on the destination mesh, the replacement bone entered in the dialog is used. Source-mesh triangles are discarded and must be rebuilt on the destination mesh.

## Current limitations

- Patch generation uses LOD 0 reference-pose geometry, not currently deformed or skinned vertices.
- After manually moving an anchor, its stored patch is stale until `Rebuild This Patch` is pressed.

## Runtime queries

Other plugins can locate the `SurfacePatches` component and query its sibling anchor components by stable `patchId`, obtain world transforms, list enabled patches, or find the closest anchor. Source and target actors keep separate components and can be matched by `patchId`.

## Presets

JSON presets live under `Resources/Presets`. Every entry has a stable ID, display name, side, bone-local location, and local outward normal. Loading a preset copies its data into the component, so runtime use does not depend on the JSON file.

Painted mesh regions will be added to the same patch entries later.

## Repository contents

Authored assets under `Content` are intentionally not ignored. Generated Unreal folders such as `Binaries`, `Intermediate`, and `Saved` are ignored.
