"""Shared setup for the example scripts: locating the Mitsuba build and
writing labelled image grids."""
import argparse
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def init_mitsuba():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--build', default=os.environ.get(
        'MI_BUILD', os.path.join(ROOT, 'build')),
        help='Mitsuba build folder (default: $MI_BUILD or <repo>/build)')
    parser.add_argument('--variant', default='llvm_ad_spectral')
    args, _ = parser.parse_known_args()
    sys.path.insert(0, os.path.join(args.build, 'python'))
    import mitsuba as mi
    if not hasattr(mi, 'set_variant'):
        sys.exit(f'Could not import Mitsuba from {args.build}/python. Build the '
                 'repository first, or pass --build / set MI_BUILD. If your '
                 'PYTHONPATH points at another Mitsuba, clear it.')
    mi.set_variant(args.variant)
    return mi


def to_srgb(img, exposure):
    x = np.clip(np.asarray(img)[..., :3] * exposure, 0, None)
    x = np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)
    return np.clip(x, 0, 1)


def save_grid(rows, path, exposure, row_labels=None, col_labels=None,
              titles=None, tile=2.0):
    """Tone-map a 2D list of HDR images and save them as one labelled figure."""
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    nr, nc = len(rows), len(rows[0])
    h, w = np.asarray(rows[0][0]).shape[:2]
    extra = 0.3 * nr if (titles or col_labels) else 0   # room for titles
    fig, axes = plt.subplots(nr, nc, figsize=(tile * nc * w / h, tile * nr + extra),
                             squeeze=False)
    for i, row in enumerate(rows):
        for j, img in enumerate(row):
            ax = axes[i][j]
            ax.imshow(to_srgb(img, exposure))
            ax.set_xticks([]); ax.set_yticks([])
            if col_labels and i == 0:
                ax.set_title(col_labels[j], fontsize=8)
            if titles and titles[i][j]:
                ax.set_title(titles[i][j], fontsize=8)
            if row_labels and j == 0:
                ax.set_ylabel(row_labels[i], fontsize=8)
    fig.tight_layout(pad=0.3)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    fig.savefig(path, dpi=110)
    plt.close(fig)
    print('wrote', os.path.relpath(path, HERE))
