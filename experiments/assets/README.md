# Scene assets

These are the files used by the paper-figure scripts. The textures
(aperture diffraction and structure factor) are not stored here. The scripts
generate them on first run into `experiments/output/textures/`.

| Path | Used by | Content |
|---|---|---|
| `glass_plate/` | `paper_fig12_glass_plate.py` | floor, wall, glass plate, two wall lamps (bulbs are the emitters) |
| `sunglasses/` | `paper_fig01_sunglasses.py` | frame, hinges and lenses of a pair of glasses |
| `envmaps/satara_night_4k.exr` | `paper_fig01_sunglasses.py` | night HDR environment map (Poly Haven, "Satara Night", CC0) |

All meshes are OBJ files in the coordinate frame the scripts expect. They were
renamed from the originals in the first author's working folder:

| File | Original name |
|---|---|
| `glass_plate/lamp_bulbs.obj` | `light2_bubble.obj` |
| `glass_plate/lamp_body.obj` | `lihgt2_metal_blonde.obj` |
| `glass_plate/lamp_part{1..4}.obj` | `test.obj`, `test2.obj`, `test3.obj`, `test4.obj` |
| `glass_plate/glass_plate.obj` | `plane_glass.obj` |
| `glass_plate/wall.obj` | `wall3.obj` |
| `glass_plate/floor.obj` | `floor.obj` |
| `sunglasses/frame.obj` | `frame_new.obj` |
| `sunglasses/hinges.obj` | `metal_new.obj` |
| `sunglasses/lenses.obj` | `glasses_new.obj` |

**TODO before release:** confirm the source and license of the lamp and
sunglasses models.
