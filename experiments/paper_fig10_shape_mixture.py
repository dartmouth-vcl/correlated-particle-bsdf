"""
Fig. 10 (simplified): mixing two aperture shapes.

Five coloured lights seen through a plate covered with a mixture of triangular
and square apertures (bounding radius a = 3 um, area fraction Fa = 0.5).
Squares give a four-pointed pattern, triangles a six-pointed one, and the
mixture interpolates between them (Eq. 28). The paper's lamp models are not
part of this repository, so the lights are small spheres.

    python paper_fig10_shape_mixture.py
"""
import argparse
import os

import numpy as np

from common import HERE, init_mitsuba, save_grid

parser = argparse.ArgumentParser()
parser.add_argument('--spp', type=int, default=512)
parser.add_argument('--res', type=int, default=320)
args, _ = parser.parse_known_args()
mi = init_mitsuba()
import wavebsdf as W  # noqa: E402

OUT = os.path.join(HERE, 'output', 'fig10')
TEX = os.path.join(HERE, 'output', 'textures')
A, FA = 3.0, 0.5
TRIANGLE_FRACTIONS = [0.0, 0.25, 0.5, 0.75, 1.0]
LIGHTS = [(-3.2, (0.10, 0.25, 1.0)), (-1.6, (0.15, 1.0, 0.25)),
          (0.0, (1.0, 1.0, 1.0)), (1.6, (1.0, 0.55, 0.05)),
          (3.2, (1.0, 0.08, 0.05))]
EXPOSURE = 1.0


def scene(bsdf):
    d = {
        'type': 'scene',
        'integrator': {'type': 'path', 'max_depth': 4},
        'sensor': {
            'type': 'perspective', 'fov': 16, 'fov_axis': 'x',
            'to_world': mi.ScalarTransform4f().look_at(
                origin=(0, -30, 0), target=(0, 0, 0), up=(0, 0, 1)),
            'sampler': {'type': 'independent', 'sample_count': args.spp},
            'film': {'type': 'hdrfilm', 'width': 2 * args.res,
                     'height': args.res, 'pixel_format': 'rgb'},
        },
        'plate': {
            'type': 'rectangle',
            'to_world': mi.ScalarTransform4f().scale((5, 1, 2.5))
                        @ mi.ScalarTransform4f().rotate((1, 0, 0), 90),
            'bsdf': bsdf,
        },
    }
    for i, (x, rgb) in enumerate(LIGHTS):
        d[f'light{i}'] = {
            'type': 'sphere', 'center': (x, 3, 0), 'radius': 0.05,
            'emitter': {'type': 'area',
                        'radiance': {'type': 'rgb', 'value': [600 * c for c in rgb]}},
        }
    return mi.load_dict(d)


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    imgs = []
    for p in TRIANGLE_FRACTIONS:
        mix = {k: v for k, v in (('triangle', p), ('square', 1 - p)) if v > 0}
        name = f'tri{int(100 * p)}_sq{int(100 * (1 - p))}'
        tex = W.make_texture_set(TEX, name, mix, A)
        path = os.path.join(OUT, name + '.exr')
        if not os.path.exists(path):
            img = mi.render(scene(mi.load_dict(W.layer_bsdf(tex, FA, A))))
            mi.util.write_bitmap(path, img, write_async=False)
            print('rendered', name)
        imgs.append(np.array(mi.Bitmap(path)))
    labels = [f'{int(100 * p)}% triangle, {int(100 * (1 - p))}% square'
              for p in TRIANGLE_FRACTIONS]
    # Aperture shapes (inscribed in the dashed bounding circle in the paper)
    h, w = imgs[0].shape[:2]
    shapes = np.zeros((h, w, 3), np.float32)
    for k, shape in enumerate(['square', 'triangle']):
        m = W.aperture_mask(shape, A, 2 * A / (0.6 * h), supersample=2)
        oy, ox = (h - m.shape[0]) // 2, w // 4 * (1 + 2 * k) - m.shape[1] // 2
        shapes[oy:oy + m.shape[0], ox:ox + m.shape[1]] = \
            m[::-1, :, None] * np.array([0.3, 0.7, 1.0]) / EXPOSURE
    save_grid([imgs[:2] + [shapes], imgs[2:]],
              os.path.join(HERE, 'output', 'fig10.png'), EXPOSURE,
              titles=[labels[:2] + [f'apertures, a = {A:g} um, Fa = {FA:g}'], labels[2:]],
              tile=2.4)
