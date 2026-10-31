# P2 partial: pressure projection on transported 3D wet cells

Date: 2026-10-31. Status: **PARTIAL**, not a P2 full acceptance.

## Reproduced numerical defect

The original opt-in `CoupledMacWater3D::step_verified` checks pressure
divergence **before** VOF transport. Transport can wet previously dry
cells, so its reported residual does not bound divergence on the
**final** wet mask. A deterministic 7 kg, seven-cell 3D water case
on 8^3 and 12^3 grids reproduces this gap. After the original verified
step, the divergence on the current VOF wet mask is about
**2.0306013986 s^-1**, despite a successful pre-transport projection.

## Implemented bounded fix

New opt-in `step_verified_reprojected` retains the existing first
verified pressure solve and performs a **second verified MAC projection**
after each VOF transport substep, with the updated wet/solid mask.
`current_wet_divergence_s_inv()` independently reconstructs the
current wet mask on a read-only MAC copy to inspect the actual residual.
The whole step remains transactional: underconvergence or invalid
input rolls back all numerical state and defers notifications.
Additional mass, finite-fraction and solid-overlap checks run before
commit. Existing `step()` and `step_verified()` paths remain unchanged.

## Reproduction and evidence

Build via MSVC Release/CMake, then execute
`build/upc_p2_post_transport_projection_tests.exe`, the full CTest
suite and `python scripts/p2_post_transport_acceptance.py`.
Source: `tests/p2_post_transport_projection3d_tests.cpp`.
Evidence: `ENGINE TEST STUBE/runs/p2_post_transport_partial.json`
and `ENGINE TEST STUBE/runs/p2_post_transport_build_ctest_20261031.txt`.

The native test verifies both grid sizes, 7 kg mass preservation,
25 wetted cells, real deferred callbacks, residual against the
read-only post-VOF diagnostic, negative timestep, NaN tolerance,
underconvergence rollback, and direct comparison with the old-mask
projection. A full MSVC Release CTest regression is required.

## Explicit limits

This adds an extra pressure solve and may increase CPU cost.
The residual is a **discrete MAC wet-cell divergence**, not an
experimental CFD validation. Reprojection does not by itself
conserve physical fluid/solid momentum or total energy. There is
no rotating body geometry, bidirectional FSI, validated free-surface
stress, external 3D CFD measurements or new UE5.8 Game/PIE Native-E2E.
**P2-P8 remain INCOMPLETE; ALL_COMPLETE=False.**
