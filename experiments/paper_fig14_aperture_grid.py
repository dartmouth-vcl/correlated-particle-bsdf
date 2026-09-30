"""
Fig. 14: diffraction through a back-lit plate covered with apertures.

3 x 3 blocks: aperture shape (circle, square, star) x bounding radius
(a = 1, 1.5, 2 um). Inside each block: area fraction Fa = 0.3, 0.5, 0.7
(columns) x plate tilt 0, 30, 60 degrees (rows). Scene layout follows the
author's generate_single_lightsource.py: a small sphere light 3 units behind a
4 x 4 plate, seen from 30 units away with a 10 degree field of view.

    python paper_fig14_aperture_grid.py            # full 9 x 9 grid
    python paper_fig14_aperture_grid.py --quick    # one tile per block
"""
import argparse
import os

import numpy as np

from common import HERE, init_mitsuba, save_grid

parser = argparse.ArgumentParser()
parser.add_argument('--spp', type=int, default=256)
parser.add_argument('--res', type=int, default=400)
parser.add_argument('--quick', action='store_true')
args, _ = parser.parse_known_args()
mi = init_mitsuba()
import wavebsdf as W  # noqa: E402

OUT = os.path.join(HERE, 'output', 'fig14')
TEX = os.path.join(HERE, 'output', 'textures')

SHAPES = ['circle', 'square', 'star']
RADII = [1.0, 1.5, 2.0]
FRACTIONS = [0.3, 0.5, 0.7]
TILTS = [0, 30, 60]
A0 = 2.0         # tabulation radius; other radii use scale = a / A0
EXPOSURE = 1.0
CROP = 0.5       # the paper shows the central part of each render


def scene(bsdf, tilt):
    return mi.load_dict({
        'type': 'scene',
        'integrator': {'type': 'path', 'max_depth': 4},
        'sensor': {
            'type': 'perspective', 'fov': 10, 'fov_axis': 'smaller',
            'to_world': mi.ScalarTransform4f().look_at(
                origin=(0, -30, 0), target=(0, 0, 0), up=(0, 0, 1)),
            'sampler': {'type': 'independent', 'sample_count': args.spp},
            'film': {'type': 'hdrfilm', 'width': args.res, 'height': args.res,
                     'pixel_format': 'rgb'},
        },
        'light': {
            'type': 'sphere', 'center': (0, 3, 0), 'radius': 0.1,
            'emitter': {'type': 'area', 'radiance': {'type': 'd65', 'scale': 220}},
        },
        'plate': {
            'type': 'rectangle',
            'to_world': mi.ScalarTransform4f().scale((2, 1, 2))
                        @ mi.ScalarTransform4f().rotate((1, 0, 0), 90 - tilt),
            'bsdf': bsdf,
        },
    })


def render(shape, a, Fa, tilt):
    tex = W.make_texture_set(TEX, shape, {shape: 1.0}, A0)
    name = f'{shape}_a{a:g}_Fa{Fa:g}_tilt{tilt}'
    path = os.path.join(OUT, name + '.exr')
    if os.path.exists(path):
        return np.array(mi.Bitmap(path))
    img = mi.render(scene(mi.load_dict(W.layer_bsdf(tex, Fa, a)), tilt))
    mi.util.write_bitmap(path, img, write_async=False)
    print('rendered', name)
    return np.array(img)


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    fr, rr, tt = (FRACTIONS[1:2], RADII, TILTS[:1]) if args.quick else \
                 (FRACTIONS, RADII, TILTS)
    def crop(img):
        h = img.shape[0]
        c = int(h * CROP / 2)
        return img[h // 2 - c:h // 2 + c, h // 2 - c:h // 2 + c]

    rows = []
    for shape in SHAPES:
        for tilt in tt:
            rows.append([crop(render(shape, a, Fa, tilt)) for a in rr for Fa in fr])
    col_labels = [f'a={a:g}um Fa={Fa:g}' for a in rr for Fa in fr]
    row_labels = [f'{s} {t} deg' for s in SHAPES for t in tt]
    save_grid(rows, os.path.join(HERE, 'output',
                                 'fig14_quick.png' if args.quick else 'fig14.png'),
              EXPOSURE, row_labels, col_labels)
