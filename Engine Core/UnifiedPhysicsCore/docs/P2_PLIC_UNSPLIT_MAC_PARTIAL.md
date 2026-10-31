# P2 – Prescribed unsplit periodic 3D MAC/PLIC flux (partial, 2026-10-31)

**Scope:** New opt-in header-only operator include/upc/p2_plic_unsplit_mac3d.hpp.
Unlike the earlier transverse-shear and direction-split operators, all three
staggered face-velocity components can vary along their own axes. The caller
provides an already divergence-free periodic 3D MAC field; no pressure
projection, Navier–Stokes momentum update or body feedback is implemented.

## Algorithm and limits

- One geometric PLIC outgoing slab per nonzero signed positive-axis face.
  The donor and receiver are determined by the face sign.
- Reconstruct periodic VOF normals from the **frozen input** and accumulate
  simultaneous fluxes into a separate delta array; input is not mutated.
- Reject non-solenoidal MAC fields, nonfinite values, invalid fractions,
  unresolved partial-cell normals and six-face absolute Courant sums > 1.
- A simple sum of independently swept axis-aligned slabs is **not** a
  geometrically exact unsplit 3D space-time volume. At corners the slabs
  overlap. This implementation **fails closed** on resulting out-of-range
  fractions; it does not silently repair them or assert global stability.
- The only clamping allowed is at the 1e-10 roundoff bound; a separate
  global volume check catches any material volume loss.

## Reproducible evidence

Run scripts/p2_plic_unsplit_build.cmd from this project. It configures
MSVC Release, builds all targets and runs the complete CTest suite.
Then run python scripts/p2_plic_unsplit_acceptance.py to verify native
numerical output, logs and source SHA256 fingerprints.

The 2026-10-31 reference run passed **104/104 CTest suites** and
**9266 native assertions** in the new test. Two exact analytic translations
of 3D water boxes (12×10×8, 20×16×12) had relative L1 errors
0.00266888888888948 and 0.00092933333333400, respectively. Those
nonzero errors expose missing diagonal corner transport.

Two genuinely three-component, spatially varying, discrete-curl MAC fields
(8³, 12×10×8) had maximum discrete divergence
1.38778e-16 and 1.24900e-16 s^-1. For uniform full water, the
analytical stationary solution was recovered at every cell; for 3D
water boxes, two successive steps conserved volume and remained bounded.
The independent tests explicitly reject a non-solenoidal field, excessive
six-face CFL, unresolved normals, invalid input, and an overlapping
multi-axis donor-slab case.

Evidence: ENGINE TEST STUBE/runs/p2_plic_unsplit_partial.json,
p2_plic_unsplit_{configure,build,ctest}_20261031.txt.

**Full phase status:** P2 remains **INCOMPLETE**, P3–P8 remain
**INCOMPLETE**. This is a prescribed-field transport experiment, not
coupled stress-consistent 3D CFD, conservative dynamic two-way FSI,
independent measured 3D fluid validation, a global mechanical energy/
momentum proof, GPU integration or Unreal 5.8 Native-E2E.
