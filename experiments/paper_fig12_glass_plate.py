"""
Fig. 12: corona from two lamps reflected and refracted by a glass plate.

Glass (BK7, eta ~ 1.5) covered with circular apertures of radius 0.8 um and
area fraction Fa = 0.5, front-lit by two strong lamps. Both reflection and
refraction contribute to the corona. Geometry, camera and materials follow the
author's scene_auto.ipynb ("scene2"); meshes are in assets/glass_plate/.

    python paper_fig12_glass_plate.py
"""
import argparse
import os

from common import HERE, init_mitsuba, save_grid

parser = argparse.ArgumentParser()
parser.add_argument('--spp', type=int, default=1024)
parser.add_argument('--width', type=int, default=960)
parser.add_argument('--assets', default=os.path.join(HERE, 'assets'))
args, _ = parser.parse_known_args()
mi = init_mitsuba()
import numpy as np  # noqa: E402
import wavebsdf as W  # noqa: E402

TEX = os.path.join(HERE, 'output', 'textures')
EXPOSURE = 0.3
T = mi.ScalarTransform4f


def mesh(name, bsdf, to_world=None, **extra):
    return {'type': 'obj', 'filename': os.path.join(args.assets, 'glass_plate', name),
            'bsdf': bsdf,
            'to_world': to_world or T().translate((0, -2, -6)), **extra}


def scene(plate_bsdf):
    metal = {'type': 'roughconductor', 'material': 'TiO2',
             'distribution': 'ggx', 'alpha_u': 0.05, 'alpha_v': 0.3}
    d = {
        'type': 'scene',
        'integrator': {'type': 'path', 'max_depth': 12},
        'sensor': {
            'type': 'perspective', 'fov': 30, 'fov_axis': 'smaller',
            'to_world': T().look_at(origin=(-110.045, 124.705, 160.127),
                                    target=(-12.318, 64.024, 24.654),
                                    up=(0, 1, 0)),
            'sampler': {'type': 'independent', 'sample_count': args.spp},
            'film': {'type': 'hdrfilm', 'width': args.width,
                     'height': args.width * 9 // 16, 'pixel_format': 'rgb'},
        },
        'bulbs': {
            'type': 'obj',
            'filename': os.path.join(args.assets, 'glass_plate', 'lamp_bulbs.obj'),
            'emitter': {'type': 'area',
                        'radiance': {'type': 'rgb', 'value': [5000, 4000, 2500]}},
            'to_world': T().translate((3, -4, -6)),
        },
        'floor': mesh('floor.obj', {'type': 'diffuse', 'reflectance': 0.1}),
        'wall': mesh('wall.obj', {'type': 'diffuse', 'reflectance':
                                   {'type': 'rgb', 'value': [0.05, 0.06, 0.06]}}),
        'lamp_body': mesh('lamp_body.obj', metal),
        'plate': mesh('glass_plate.obj', plate_bsdf,
                      T().translate((0, -2, -6)) @ T().scale(1.2)),
    }
    for i in range(1, 5):
        d[f'lamp_part{i}'] = mesh(f'lamp_part{i}.obj', metal)
    return mi.load_dict(d)


if __name__ == '__main__':
    out = os.path.join(HERE, 'output', 'fig12')
    os.makedirs(out, exist_ok=True)
    tex = W.make_texture_set(TEX, 'circle', {'circle': 1.0}, 2.0)
    imgs = []
    for name, bsdf in [
            ('clear_glass', {'type': 'dielectric', 'int_ior': 'bk7', 'ext_ior': 'air'}),
            ('apertures', W.layer_bsdf(tex, Fa=0.5, a=0.8,
                                       int_ior='bk7', ext_ior='air'))]:
        path = os.path.join(out, name + '.exr')
        if not os.path.exists(path):
            img = mi.render(scene(bsdf))
            mi.util.write_bitmap(path, img, write_async=False)
            print('rendered', name)
        imgs.append(np.array(mi.Bitmap(path)))
    save_grid([imgs], os.path.join(HERE, 'output', 'fig12.png'), EXPOSURE,
              titles=[['clear glass', 'glass with apertures (a = 0.8 um, Fa = 0.5)']],
              tile=3.6)
