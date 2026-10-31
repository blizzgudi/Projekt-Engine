# P2 — VOF-weighted angular-momentum / inertia diagnostic (partial)

Date: 2026-10-31. Scope: **read-only diagnostic** on a 3D MAC grid with
VOF water fractions and a fixed solid mask. This is NOT a full P2 acceptance.

## Implementation

`include/upc/p2_vof_angular_inventory3d.hpp` provides
`vof_angular_inventory3d` and `vof_angular_delta3d`. Each occupied
cell is approximated by a point mass `m=rho*dx^3*fill` at the cell centre.
Velocity is the mean of the two neighbouring MAC faces per axis.

* Linear momentum: `sum(m*v)`.
* Angular momentum about configurable origin `o`: `sum((r-o) cross (m*v))`.
* Mass centroid in world coordinates: `sum(m*r)/sum(m)`.
* Inertia about origin: `sum(m*(|r-o|^2 I - (r-o)(r-o)^T))`.
* Inertia about centroid: subtract the parallel-axis term.
* Angular delta: after minus before, without asserting conservation.

No intrinsic cell-cube rotational inertia is added: that would be
inconsistent with the cell-centre velocity sampling. The centroid of an
empty domain is defined as the supplied origin, with zero moments.

## Independently derived reference

On two 3D grids (8^3 and 12^3), `dx=0.1 m`, `rho=1000 kg/m^3`,
two cells at (2,2,2) and (5,5,5) have VOF .25 and .75 and
velocities (2,-1,0.5) and (-1,0,-2) m/s. Their masses are .25/.75 kg,
positions (.25,.25,.25)/(.55,.55,.55) m.

* Total mass: 1 kg; centroid: (.475,.475,.475) m.
* Linear momentum: (-.25,-.25,-1.375) kg m/s.
* Angular momentum about zero: (-.73125,.50625,.225) kg m^2/s.
* Inertia about centroid: diagonal .03375, off-diagonal -.016875 kg m^2.
* Inertia about zero: diagonal .485, off-diagonal -.2425 kg m^2.
* Shifting the origin to (.1,-.2,.3) changes angular momentum to
  (-1.08125,.44375,.3), as `L'=L-a cross P` requires.
* Moving .125 kg between these cells at fixed sampled velocities
  produces angular delta (-.184375,.021875,.1625) kg m^2/s.

The native test additionally verifies dry-ghost independence, invalid
VOF and solid overlap rejection, nonfinite origin rejection, empty
domains, and a **nontrivial residual-verified MAC pressure projection**
on two adjacent fluid cells with unequal partial fill. The projection
produces a measurable angular-inventory change, which is **not**
identified as a physical torque or solid reaction.

## Reproducibility

* `scripts/p2_vof_angular_full_build.cmd` — MSVC Release and full CTest.
* `scripts/p2_vof_angular_acceptance.py` — independent quantitative
  CSV references and source/binary/CTest SHA-256 checks.
* `ENGINE TEST STUBE/runs/p2_vof_angular_partial.json` — machine-readable
  report, generated only by the acceptance script.
* `ENGINE TEST STUBE/runs/p2_vof_angular_{build,ctest}_20261031.txt` —
  build and full-suite evidence.

## Limitations / open acceptance criteria

This is a sampled point-mass budget, not exact finite-volume angular
momentum or a conservation proof. The pressure-projection test does not
apply an equal-and-opposite torque to a moving body. Stress-consistent
free surfaces, rotating moving-solid FSI, independent measured 3D CFD
reference data, global mass/linear/angular impulse and energy closure,
GPU/LOD physics and UE5.8 Native-E2E for this diagnostic remain open.
P2–P8 stay **INCOMPLETE**; `ALL_COMPLETE=False`.
