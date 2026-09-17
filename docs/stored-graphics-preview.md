# Stored graphics map preview

Use your own legally obtained game files:

```sh
./build/openemperor --data .local/gog-extracted/app --map-debug Cities/Xia.map --view stored-graphics --graphics-profile exe-6373328b-v213-runtime-table
```

This opt-in view shows the **saved graphic ID** at each cell of the existing diagnostic candidate mask. It uses the corrected, shared version-213 Terrain/Elevation runtime table to resolve slot, local index, and physical SG3 image record; no terrain binding file or terrain-category lookup is involved. The map's saved words, candidate bytes, terrain words, object words, off-map bits, and storage coordinates are left unchanged. Storage cells outside the candidate mask remain excluded, including mask/off-map-bit disagreements; the preview does not declare a playable area.

Only a supported single Emperor Type-30 footprint is drawn: type 30, width 78, height at least 40, 3,200-byte base, consistent one-cell decoder geometry, valid normal color/alpha source, and no nonzero unverified mirror offset. The existing decoder returns one RGBA image with its optional Omega overlay. At projected storage cell `(x,y)` and geometry border `b`, the preview uses `world=((x-b)-(y-b))*40, ((x-b)+(y-b))*20` and image origin `world-(width/2,height-40)`. Thus 78×40, 78×46, 78×48, and 78×54 images all keep the same lower footprint position and display their complete upper extent. This is a preview convention derived from the decoder geometry, **not** a proven original-game placement or reconstructed terrain height.

Unsupported sprites, more-than-one-cell footprints, unverified mirrors, empty records, missing sources, and failed decodes appear as purple diagnostic diamonds in the same depth order as pictures. The title identifies the mode as “Stored graphics preview”. Click or press Return to select a logical storage cell; the console reports the saved ID in hex and decimal, map logical offset, candidate byte, slot/local index, system skip, physical record, archive, type, dimensions, anchor, overlay presence, source bounds, and attempted/successful decode status. WASD/arrows pan, the wheel zooms around the pointer, `R` refits, and `V` cycles the existing map views. Selection follows the logical cell footprint rather than upper-image pixels.

The mode checks all used archive and bitmap paths under the selected data root, including symlinks. It allows at most 2,048 distinct referenced assets, 16 MiB logical decoded RGBA per image, and 64 MiB summed logical RGBA texture size. These bounds are **not** exact driver/GPU memory measurements. Each distinct eligible image is decoded and uploaded once; camera movement does not reopen files or upload textures. A missing required core archive or invalid profile fails loading. A per-image decode failure is retained as a diagnostic status. No original or decoded asset is written by the viewer.

The saved array is known to be loaded into a related graphics lookup path, but later original-game writes before first draw are unresolved. This snapshot does not reproduce the first frame, animation, multi-cell placement, buildings, coastline decisions, or simulation. The independent [static evidence](reverse/graphics-id.md) and [architecture](architecture.md) keep those limits explicit.
