# Examples and paper figures

See the [top-level README](../README.md) for how to run the scripts. This page
covers the conventions they share and how the results were checked.

| File | Purpose |
|---|---|
| `wavebsdf.py` | aperture textures (FFT), structure factor, and `layer_bsdf()` |
| `common.py` | finds the Mitsuba build; writes labelled image grids |
| `quickstart.py` | minimal corona example, no external assets |
| `paper_fig*.py` | one script per reproduced paper figure |
| `assets/` | meshes and environment map for Figs. 1 and 12 (see `assets/README.md`) |
| `figures/` | the images shown in the top-level README |
| `output/` | renders and generated textures (git-ignored, regenerated on demand) |

## Conventions (must match `particle2d_IS_texture.cpp`)

* **Reference wavelength.** λ0 = 350 nm. A lookup at wavelength λ scales the
  coordinates by λ̂ = λ0/λ.
* **`Ia` texture.** Texel `(i, j)` stores `T_a(d)` at the texel centre
  `d = ((i, j) + 0.5)/res · 4 − 2`, i.e. `uv = d/4 + 0.5`. Following
  Sec. 7.1, `T_a(d) = |A|⁻¹ |F{A}(2π d/λ0)|² λ0⁻²`, so `∫ T_a dd = 1`. The
  FFT grid uses a spacing of λ0/4 and a half-bin phase shift so that samples
  land on texel centres. For a circle it matches the analytic Airy pattern to
  within 0.03%.
* **`values`.** The structure factor `S'(k)` (Eqs. 15–16) is sampled at
  `k = 2π d/λ0` for 5000 values of `d ∈ [0, 2]`. The plugin reads it as a
  `ContinuousDistribution` and uses the unnormalized `eval_pdf`, which is
  the raw tabulated value.
* **Scale.** `scale = a / a0` renders radius `a` from textures tabulated at
  `a0`. Both `T_a` and `S'` are rescaled.
* **Shape mixtures (Eq. 28).** These are implemented exactly as
  `T_coh · S' + T_inc` with two plugin instances blended 50/50. The textures
  are stored pre-doubled to compensate for the blend.

## Validation

* **Energy.** `∫ f* cos θo dω = 1/(1 − F_r)` at every wavelength (checked
  numerically with `S' = 1`), as Eq. (29) implies.
* **`blendbsdf`.** `eval` equals `(1 − w) f0 + w f1`, and the sampled
  albedo matches it for `w ∈ {0.1, 0.3, 0.5, 0.9}`.
* **Against the first author's renders**
  (`resources/data/scenes/generate_single_light/*/exr`, same scene):
  - Circles match in shape and absolute brightness.
  - Squares and stars come out about 2× brighter here. The author's nested
    blend gave the diffraction lobe a weight of `(1 − Fa)·F_r`, while
    Eq. (20) specifies `F_r − F_r²`. These agree only for circles, where
    `F_r = Fa`.

## History of the code base

The work was handed over as uncommitted changes on top of Mitsuba 3
`d394bc40`. Before release:

* **Portability.** Replaced `M_PI` with `dr::Pi`, which fixes builds with
  standard compilers.
* **`blendbsdf`.** `eval_pdf` returned `(1−w)²/(1−nw)·f0 + w²/nw·f1`. It
  now returns `(1−w)·f0 + w·f1`, the `m1` sampling branch writes its pdf
  under the correct mask, and `pdf()` uses the same selection probabilities
  as sampling.
* **Brightness constant.** Removed the `×1000` factor in
  `particle2d_IS_texture`. It compensated for 8-bit textures normalized to a
  peak of 1. Textures are now in the physical units of Sec. 7.1.
* **Debug output.** Removed prints from the new plugins. Restored upstream
  `conductor`, `diffuse`, `homogeneous` and `rayleigh`, whose only changes
  were debug leftovers.

The author's working files (scenes, notebooks, earlier renders) are in the
`resources/data` submodule, untracked. They are not part of the release.
Their scene notebook `resources/data/scenes/scene_auto.ipynb` is the source
of the geometry used for Figs. 1 and 12. The meshes it uses were copied, with
new names, into `assets/`.
