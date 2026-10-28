# P2 PARTIAL – Translating-box cut-cell MAC pressure/VOF continuity (28.10.2026)

## Scope
The new opt-in P2MovingBoxCutCellProjection3DBuilder::sample takes exact
moving-box cut-cell volumes, time-averaged MAC apertures and geometric swept-wall
volumes from existing P2 components. It solves a *closed-domain, initially
stationary, constant-density* pressure correction with a weighted graph
Laplacian (Jacobi-preconditioned conjugate gradient). On every open internal
face, the resulting time-integrated fluid flux is
Q_ij = aperture_ij * (phi_i - phi_j) (m³). For each cell the solver
enforces sum(outward Q_fluid) = (alpha_old-alpha_new)*dx³.
The pressure conversion p = rho*phi/(dt²*dx) is consistent with a
single projection from zero initial velocity, not a complete fluid model.
Disconnected sealed regions are checked for individual volume compatibility.
The solver never changes its input boxes or any live fluid/solid state.

## Native verification
tests/p2_moving_box_cut_cell_projection3d_tests.cpp covers 8³, 12³,
16³ grids; all three axes, both directions, and 0.25/0.5 cell shifts.
It checks the moving cut-cell geometry against an independently coded
box/cell overlap, every local flux divergence against geometric volume
change, zero exterior flux, convergence and pressure finiteness.
The solved MAC flux is also passed to the existing
FractionalBoundaryVofFlux3D with an initially fully saturated fluid:
the VOF update conserves water and reproduces every new open-cell
volume within tolerance, with donor CFL/boundedness checks.
Stationary geometry and invalid density, box movement and
iteration budget are rejected or return the correct zero state.
Independent Python verification compares all 36 analytic total
changed-volume references: L1 = 2*(1.5m*1.5m)*shift for these tests.

Evidence: ENGINE TEST STUBE/runs/p2_cut_cell_projection_partial.json,
ENGINE TEST STUBE/runs/p2_cut_cell_projection_full_ctest_20261028.txt,
scripts/p2_cut_cell_projection_acceptance.py, plus SHA-256 signatures.
Only the *new bounded partial* is accepted, never the P2 full phase.

## Important limitations
- This is a single geometric pressure correction for a *closed and
  completely saturated* domain, from zero predicted velocity. It is not
  the full MAC pressure step with arbitrary advected velocities,
  free-surface pressure or viscous stress.
- The cut-cell apertures are time-averaged for an axis-aligned linearly
  translating box. No rotation, body deformation, multiple-body contact,
  independent 3D CFD experiment, or fluid-driven rigid-body trajectory.
- The successful VOF example is saturated. Arbitrary partial filling
  requires explicit free-surface pressure/flux constraints; the donor-CFL
  may reject physically unsupported input states.
- No physically validated fluid/solid impulse feedback or total
  mechanical/thermal energy accounting; no new UE5.8 Native-E2E.
- **P2–P8 remain PARTIAL.** The next step is to integrate the projection
  into a time-resolved MAC/VOF step with moving-wall velocity and to
  validate post-projection impulse/energy, not just geometry.
