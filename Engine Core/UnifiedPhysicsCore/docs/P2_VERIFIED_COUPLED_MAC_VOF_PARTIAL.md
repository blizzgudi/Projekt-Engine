# P2 partial: transactional verified pressure in the live 3D MAC/VOF solver

Date: 2026-10-31. Scope: **partial P2 CPU acceptance only**.

'CoupledMacWater3D::step_verified(seconds, iterations, max_divergence_s_inv)'
now integrates the previously isolated 'MacProjection3D::project_verified'
into the actual coupled advection, gravity, pressure and VOF transport loop.
Each adaptive substep checks actual wet-cell divergence. The entire public
step runs on a private copy; failed numerical convergence leaves the original
water fractions, pressure, velocities, diagnostics and transport notifications
unchanged. A successful commit replays deferred transport notifications.
The original 'step()' remains unchanged for existing callers.

The callback replay is **not** an external transactional system: if a user
callback throws after the numerical state is committed, the callback's
external effects cannot be undone. This API does not claim that guarantee.

Native tests use two distinct 3D grids (8^3, 12^3) with seven filled water
voxels and a solid bottom neighbor. The one-iteration pressure solve is
deliberately under-converged and must roll back. The successful solve is
compared cell-by-cell to the legacy solver for water fraction, pressure,
mass, diagnostics, and notification payloads. The notification handler
checks that the committed state is visible before delivery. Invalid zero
and NaN tolerances are rejected without mutation.

Reproduce: 'scripts/p2_verified_coupled_full_build.cmd', followed by
'python scripts/p2_verified_coupled_acceptance.py'. Versioned evidence:
'ENGINE TEST STUBE/runs/p2_verified_coupled_partial.json' and
'p2_verified_coupled_ctest_20261031.txt'. Source, binary and CTest SHA256
values are included in the JSON.

This is **not** a physical fluid/rigid-body conservation proof. The live
solver still lacks moving/rotating body geometry with bidirectional
impulse/energy conservation, stress-correct free surfaces, independent
3D CFD measurement validation, and a new UE5.8 native GAME+PIE E2E
run for this opt-in path. P2-P8 remain INCOMPLETE.
