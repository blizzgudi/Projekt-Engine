# P2 PARTIAL — Moving-box pressure counterimpulse on MAC dual masses (29.10.2026)

## Scope and implementation

`include/upc/p2_moving_box_mac_counterimpulse3d.hpp` composes the previously
tested frozen-pressure dynamic rigid-box translation with **an actual update
of copied staggered MAC face velocities**. It uses exact old/new moving-box
dual open-volume fractions and time-averaged primal face apertures. For each
axis it chooses open interior duals within one cell of the translating box's
midpoint exterior, assigns the mass `rho * dx^3 * (alpha_old+alpha_new)/2`,
and deposits the opposite pressure impulse as one uniform velocity increment
on the selected component faces. All three components are handled separately.

The component is opt-in and returns updated MAC face arrays without changing
caller inputs or the existing `CoupledMacWater3D` live solver. It checks
shapes, closed exterior normal faces, finite fields, rigid pressure geometry,
positive dual mass and finite energy/impulse accumulations before returning.

**This is not a physically closed FSI solver.** Selected face momentum is a
discrete mass-weighted proxy. The fluid-side impulse is applied to the returned
MAC field, but there is no subsequent cut-cell pressure projection or
free-surface VOF transport. The computed per-cell geometric-continuity
residual is reported and is expected to be nonzero in the test cases.
Neither a physical global momentum tensor nor fluid+solid energy conservation
has been established. The kinetic-energy change is explicitly a selected
dual-face proxy, not an independent mechanical energy balance.

## Reproducible evidence

Run `scripts/p2_moving_box_mac_counterimpulse_full_build.cmd` from the
project to configure/build the MSVC Release target and execute the entire
CTest suite, saving the complete log in
`ENGINE TEST STUBE/runs/p2_moving_box_mac_counterimpulse_full_ctest_20261029.txt`.
Then run `python scripts/p2_moving_box_mac_counterimpulse_acceptance.py`.
The script independently checks analytic two-step pressure-gradient impulse
and body trajectories for 8^3/12^3 grids, three axes, two signs, hashes the
sources and log, and writes
`ENGINE TEST STUBE/runs/p2_moving_box_mac_counterimpulse_partial.json`.

The native tests additionally cover a pressure-gauge shift, mass-weighted
face kinetic-energy identity, input rollback on exceptions, nonfinite values,
invalid density/shape/boundaries, and explicitly nonzero GCL residual.
Only report counts from a successful real build and run.

## Remaining P2 full acceptance gaps

A coupled 3D MAC/VOF time step must transport both mass and momentum through
the same conservative physical fluxes, apply a stress-consistent moving-wall
pressure/viscous force and its equal/opposite rigid-body reaction, then
reproject the shared fluid field without losing the global momentum budget.
It must close physical kinetic/thermal/work energy and water mass balances,
include free-surface tension/stress and wet/soil interactions where relevant,
and be compared with independent measured 3D CFD references. Native UE5.8
Game+PIE E2E for this new coupled path remains outstanding. P2 and P3–P8
remain PARTIAL.
