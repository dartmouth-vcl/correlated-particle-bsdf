"""
Fig. 1 (bottom right): condensation on a pair of glasses.

The lenses are BK7 glass covered with circular apertures (a = 5 um,
Fa = 0.5), lit by an environment map with bright street lamps; every lamp seen
through (or reflected in) the lenses gets a corona. Geometry, camera and
materials follow the author's scene_auto.ipynb ("scene1"); meshes and the
environment map are in assets/sunglasses/ and assets/envmaps/.

    python paper_fig01_sunglasses.py
"""
import argparse
import os

from common import HERE, init_mitsuba, save_grid

parser = argparse.ArgumentParser()
parser.add_argument('--spp', type=int, default=1024)
parser.add_argument('--width', type=int, default=960)
parser.add_argument('--assets', default=os.path.join(HERE, 'assets'))
parser.add_argument('--envmap', default='satara_night_4k.exr',
                    help='environment map in assets/envmaps/')
args, _ = parser.parse_known_args()
mi = init_mitsuba()
import numpy as np  # noqa: E402
import wavebsdf as W  # noqa: E402

TEX = os.path.join(HERE, 'output', 'textures')
EXPOSURE = 3.0
T = mi.ScalarTransform4f
# Placement of all glasses parts, copied from the author's notebook
GLASSES = (T().rotate((1, 0, 0), -90) @ T().rotate((0, 0, 1), -179)
           @ T().rotate((0, 1, 0), -5) @ T().translate((0, -2, -6)))


def scene(lens_bsdf):
    part = lambda name, bsdf: {  # noqa: E731
        'type': 'obj', 'filename': os.path.join(args.assets, 'sunglasses', name),
        'bsdf': bsdf, 'to_world': GLASSES}
    return mi.load_dict({
        'type': 'scene',
        'integrator': {'type': 'path', 'max_depth': 12},
        'sensor': {
            'type': 'perspective', 'fov': 60, 'fov_axis': 'smaller',
            'to_world': T().look_at(origin=(2.5, -5.5, 0.001),
                                    target=(2.5, 1, 0), up=(0, 1, 0)),
            'sampler': {'type': 'independent', 'sample_count': args.spp},
            'film': {'type': 'hdrfilm', 'width': args.width,
                     'height': args.width * 9 // 16, 'pixel_format': 'rgb'},
        },
        'envmap': {
            'type': 'envmap',
            'filename': os.path.join(args.assets, 'envmaps', args.envmap),
            'to_world': mi.ScalarTransform4f([
                [-0.224951, -0.000001, -0.974370, 0.0],
                [-0.974370, 0.000000, 0.224951, 0.0],
                [0.000000, 1.000000, -0.000001, 8.87],
                [0.0, 0.0, 0.0, 1.0]]) @ T().rotate((0, 1, 0), 70),
        },
        'frame': part('frame.obj', {'type': 'diffuse', 'reflectance':
                                        {'type': 'rgb', 'value': [0.05, 0.05, 0.1]}}),
        'metal': part('hinges.obj', {'type': 'roughconductor', 'material': 'TiO2',
                                        'distribution': 'ggx',
                                        'alpha_u': 0.05, 'alpha_v': 0.3}),
        'lenses': part('lenses.obj', lens_bsdf),
    })


if __name__ == '__main__':
    out = os.path.join(HERE, 'output', 'fig01', os.path.splitext(args.envmap)[0])
    os.makedirs(out, exist_ok=True)
    tex = W.make_texture_set(TEX, 'circle', {'circle': 1.0}, 5.0)
    imgs = []
    for name, bsdf in [
            ('clear_lenses', {'type': 'dielectric', 'int_ior': 'bk7', 'ext_ior': 'air'}),
            ('condensation', W.layer_bsdf(tex, Fa=0.5, a=5.0,
                                          int_ior='bk7', ext_ior='air'))]:
        path = os.path.join(out, name + '.exr')
        if not os.path.exists(path):
            img = mi.render(scene(bsdf))
            mi.util.write_bitmap(path, img, write_async=False)
            print('rendered', name)
        imgs.append(np.array(mi.Bitmap(path)))
    save_grid([imgs], os.path.join(HERE, 'output', 'fig01.png'), EXPOSURE,
              titles=[['clear lenses', 'condensation (a = 5 um, Fa = 0.5)']],
              tile=3.6)
