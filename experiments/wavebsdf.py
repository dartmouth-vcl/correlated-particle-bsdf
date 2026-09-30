"""
Helpers for the wave-optics BSDF of

    R. Yang, J. Kim, A. Pediredla, W. Jarosz,
    "A wave-optics BSDF for correlated scatterers", EGSR / CGF 44(4), 2025.

The Mitsuba plugin `particle2d_IS_texture` evaluates Eq. (29) of the paper:

    f*(wi, wo, lambda) = cos(theta_i)/cos(theta_o) * T_a(lh q d) * T_s(lh q |d|)
                         * (eta lh q)^2 / (1 - F_r),         lh = lambda_0 / lambda

and needs two precomputed inputs, both tabulated at lambda_0 = 0.35 um for a
scatterer of bounding radius `a0`:

    Ia     : aperture diffraction texture T_a(d),  d in (-2, 2)^2   (Sec. 7.1)
    values : structure factor S'(d) of a hard-disk process, d in (0, 2)  (Eq. 15)

`scale` (= q) rescales the tabulated system, so the effective radius is a0*q.
This module builds both inputs and assembles the complete layer BSDF with the
component weights w* : w_delta : w_o = F_r - F_r^2 : F_r^2 : 1 - F_r (Eq. 20).
"""
import os

import numpy as np
from scipy.special import j0, j1

LAMBDA0 = 0.35  # um, reference wavelength hard-coded in the plugin


# ---------------------------------------------------------------------------
# Structure factor (Sec. 5.2)
# ---------------------------------------------------------------------------
def percus_yevick_2d(k, a, Fa):
    """Hard-disk structure factor S'(k | Fa) (Eqs. 15-16) for bounding radius
    `a` (um), wavenumber magnitude `k` (1/um) and area fraction `Fa`."""
    x = np.maximum(np.asarray(k, dtype=np.float64) * a, 1e-9)
    A = 2 * j1(x) / x
    C = (4 * Fa / (1 - Fa) * j1(2 * x) / x
         + 4 * Fa**2 / (1 - Fa)**2 * j0(x) * j1(x) / x
         + (Fa / (1 - Fa)**2 + 2 * Fa**2 / (1 - Fa)**3) * A**2)
    return 1 / (1 + C)


def structure_factor_values(Fa, a0, n=5000):
    """`values` string for the plugin: S'(k = 2 pi d / lambda_0) on d in [0, 2]."""
    d = np.linspace(1e-6, 2, n)
    S = percus_yevick_2d(2 * np.pi * d / LAMBDA0, a0, Fa)
    return ', '.join(f'{v:.6g}' for v in S)


# ---------------------------------------------------------------------------
# Aperture shapes and diffraction textures (Secs. 4.1 and 7.1)
# ---------------------------------------------------------------------------
def shape_polygon(shape, a):
    """Vertices of an aperture inscribed in a bounding circle of radius `a`,
    or None for a circle."""
    if shape == 'circle':
        return None
    if shape in ('square', 'rectangle'):
        t = np.pi / 4 + np.arange(4) * np.pi / 2
    elif shape == 'triangle':
        t = np.pi / 2 + np.arange(3) * 2 * np.pi / 3
    elif shape == 'star':  # 5-pointed star, inner/outer radius ratio 0.4
        t = np.pi / 2 + np.arange(10) * np.pi / 5
        r = np.where(np.arange(10) % 2 == 0, a, 0.4 * a)
        return np.stack([r * np.cos(t), r * np.sin(t)], -1)
    else:
        raise ValueError(f'unknown shape {shape!r}')
    return np.stack([a * np.cos(t), a * np.sin(t)], -1)


def _inside_polygon(x, y, poly):
    inside = np.zeros(x.shape, bool)
    n = len(poly)
    for i in range(n):
        (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % n]
        crosses = (y0 > y) != (y1 > y)
        xint = x0 + (y - y0) * (x1 - x0) / (y1 - y0 + 1e-30)
        inside ^= crosses & (x < xint)
    return inside


def aperture_mask(shape, a, dx, supersample=8):
    """Anti-aliased aperture A(x) on a local grid with spacing `dx` (um)."""
    half = int(np.ceil(a / dx)) + 2
    m = 2 * half
    s = (np.arange(m * supersample) + 0.5) / supersample - half
    x, y = np.meshgrid(s * dx, s * dx)
    poly = shape_polygon(shape, a)
    inside = (x**2 + y**2 <= a * a) if poly is None else _inside_polygon(x, y, poly)
    return inside.reshape(m, supersample, m, supersample).mean(axis=(1, 3))


def aperture_spectrum(shape, a, res):
    """|F{A}(2 pi d / lambda_0)| sampled at texel centres of a res x res grid
    covering d in (-2, 2)^2, together with the aperture area |A| (um^2)."""
    dx = LAMBDA0 / 4                      # makes the FFT span d in (-2, 2)
    local = aperture_mask(shape, a, dx)
    grid = np.zeros((res, res), np.complex128)
    m = local.shape[0]
    o = res // 2 - m // 2
    grid[o:o + m, o:o + m] = local
    # Shift by half a frequency bin, X[k] -> X(k + 1/2), so that after
    # fftshift sample m lies at the texel centre d = (m + 0.5)/res * 4 - 2
    ramp = np.exp(-1j * np.pi * np.arange(res) / res)
    grid *= ramp[None, :] * ramp[:, None]
    F = np.fft.fftshift(np.fft.fft2(grid)) * dx * dx
    return np.abs(F), local.sum() * dx * dx


def aperture_textures(mixture, a0, res=2048):
    """Diffraction textures for a mixture {shape: P(m)} of apertures sharing
    the bounding radius `a0` (Eq. 28), normalized as in Sec. 7.1.

    Returns (T_coh, T_inc, area) with
        T_coh = (sum_m P F_m)^2               / (|A| lambda_0^2)   (multiplies S')
        T_inc = (sum_m P F_m^2 - (sum_m P F_m)^2) / (|A| lambda_0^2)  (no S')
    where |A| = sum_m P |A_m| is the mean aperture area. For a single shape
    T_inc == 0 and T_coh is the paper's T_a (integrates to 1 over d).
    """
    total = sum(mixture.values())
    mean_F = mean_F2 = 0
    area = 0
    for shape, p in mixture.items():
        F, A = aperture_spectrum(shape, a0, res)
        w = p / total
        mean_F = mean_F + w * F
        mean_F2 = mean_F2 + w * F * F
        area += w * A
    norm = 1 / (area * LAMBDA0**2)
    T_coh = mean_F**2 * norm
    T_inc = np.maximum(mean_F2 - mean_F**2, 0) * norm
    return T_coh.astype(np.float32), T_inc.astype(np.float32), area


def write_texture(path, T):
    import mitsuba as mi
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    mi.Bitmap(np.ascontiguousarray(T[..., None])).write(path)
    return path


def make_texture_set(out_dir, name, mixture, a0, res=2048):
    """Write the textures for `mixture` once and cache them on disk."""
    coh = os.path.join(out_dir, f'{name}_a{a0:g}_coh.exr')
    inc = os.path.join(out_dir, f'{name}_a{a0:g}_inc.exr')
    meta = os.path.join(out_dir, f'{name}_a{a0:g}_area.txt')
    if not (os.path.exists(coh) and os.path.exists(meta)):
        T_coh, T_inc, area = aperture_textures(mixture, a0, res)
        if T_inc.max() > 0:
            # layer_bsdf blends the two lobes 50/50, which halves each of them
            write_texture(coh, 2 * T_coh)
            write_texture(inc, 2 * T_inc)
        else:
            write_texture(coh, T_coh)
        with open(meta, 'w') as f:
            f.write(f'{area:.10g}\n')
    area = float(open(meta).read())
    return {'coh': coh, 'inc': inc if os.path.exists(inc) else None,
            'area': area, 'a0': a0}


# ---------------------------------------------------------------------------
# BSDF assembly (Sec. 6, Fig. 5)
# ---------------------------------------------------------------------------
def _particle(Ia, values, Fr, scale, int_ior, ext_ior):
    return {
        'type': 'particle2d_IS_texture',
        'naive': 1,
        'f': Fr,
        'scale': scale,
        'values': values,
        'int_ior': int_ior,
        'ext_ior': ext_ior,
        'Ia': {'type': 'bitmap', 'filename': Ia, 'raw': True,
               'wrap_mode': 'clamp', 'filter_type': 'bilinear'},
    }


def layer_bsdf(tex, Fa, a, int_ior='air', ext_ior='air', substrate=None):
    """Complete BSDF of a surface covered with apertures/scatterers.

    tex       : output of make_texture_set (tabulated at radius tex['a0'])
    Fa        : area fraction of the bounding circles (hard-disk packing)
    a         : bounding radius to render (um); scale q = a / a0
    substrate : BSDF dict for light that misses the apertures (f_r^o), e.g. a
                diffuse fabric. None means the plate is a clear interface
                (dielectric int_ior/ext_ior).
    """
    a0 = tex['a0']
    q = a / a0
    Fr = Fa * tex['area'] / (np.pi * a0**2)   # real covered fraction
    values = structure_factor_values(Fa, a0)
    clear = {'type': 'dielectric', 'int_ior': int_ior, 'ext_ior': ext_ior}

    particles = _particle(tex['coh'], values, Fr, q, int_ior, ext_ior)
    if tex['inc'] is not None:
        # Mixture of shapes, Eq. (28) = T_coh * S' + T_inc (textures pre-doubled)
        inc = _particle(tex['inc'], '1, 1', Fr, q, int_ior, ext_ior)
        particles = {'type': 'blendbsdf', 'weight': 0.5,
                     'bsdf_0': particles, 'bsdf_1': inc}

    if substrate is None:
        rest = clear                       # delta and substrate both pass straight
    else:
        w_delta, w_sub = Fr**2, 1 - Fr
        rest = {'type': 'blendbsdf', 'weight': w_sub / (w_delta + w_sub),
                'bsdf_0': clear, 'bsdf_1': substrate}

    w_star = Fr - Fr**2
    return {'type': 'blendbsdf', 'weight': 1 - w_star,
            'bsdf_0': particles, 'bsdf_1': rest}
