"""
Quick start: the corona around a light seen through a particle-coated plate.

Renders the same small light through a clear plate and through plates covered
with circular apertures of increasing radius. Runs in a few seconds on a CPU
and needs no external assets.

    python quickstart.py
"""
import argparse
import os

from common import HERE, init_mitsuba, save_grid

parser = argparse.ArgumentParser()
parser.add_argument('--spp', type=int, default=128)
parser.add_argument('--res', type=int, default=320)
args, _ = parser.parse_known_args()
mi = init_mitsuba()
import numpy as np  # noqa: E402
import wavebsdf as W  # noqa: E402

T = mi.ScalarTransform4f


def render(plate_bsdf):
    scene = mi.load_dict({
        'type': 'scene',
        'integrator': {'type': 'path'},
        'sensor': {
            'type': 'perspective', 'fov': 10,
            'to_world': T().look_at(origin=(0, -30, 0), target=(0, 0, 0),
                                    up=(0, 0, 1)),
            'sampler': {'type': 'independent', 'sample_count': args.spp},
            'film': {'type': 'hdrfilm', 'width': args.res, 'height': args.res},
        },
        # small light 3 units behind the plate
        'light': {'type': 'sphere', 'center': (0, 3, 0), 'radius': 0.1,
                  'emitter': {'type': 'area',
                              'radiance': {'type': 'd65', 'scale': 220}}},
        # 4 x 4 plate facing the camera
        'plate': {'type': 'rectangle',
                  'to_world': T().scale((2, 1, 2)) @ T().rotate((1, 0, 0), 90),
                  'bsdf': plate_bsdf},
    })
    return np.array(mi.render(scene))


if __name__ == '__main__':
    # Diffraction texture of one circular aperture (tabulated at a0 = 2 um)
    tex = W.make_texture_set(os.path.join(HERE, 'output', 'textures'),
                             'circle', {'circle': 1.0}, a0=2.0)
    images, titles = [render({'type': 'dielectric', 'int_ior': 'air',
                              'ext_ior': 'air'})], ['clear plate']
    for a in [1.0, 1.5, 2.0]:
        images.append(render(W.layer_bsdf(tex, Fa=0.5, a=a)))
        titles.append(f'apertures a = {a:g} um, Fa = 0.5')
    save_grid([images], os.path.join(HERE, 'output', 'quickstart.png'),
              exposure=1.0, titles=[titles], tile=2.6)
