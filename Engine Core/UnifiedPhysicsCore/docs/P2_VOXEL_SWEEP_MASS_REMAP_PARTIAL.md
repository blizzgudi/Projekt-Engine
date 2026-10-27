# P2 PARTIAL - One-cell 3D solid-mask sweep with conservative VOF reclassification (27.10.2026)

**Status:** P2 remains PARTIAL; this is NOT dynamic bidirectional FSI.

## Scope

New standalone header include/upc/p2_voxel_sweep_remap3d.hpp implements an exact
integer one-voxel translation of a rigid occupancy mask along one signed axis.
The operation is transactional: inputs are const and failures return no result.
Before any water reclassification, it validates domain dimensions, physical
cell size, array lengths, mask values, nonfinite/out-of-range VOF fractions,
water/solid overlap, fixed-solid collisions and body exits at domain boundaries.

For each old contiguous solid run parallel to the translation direction,
water fraction from the newly occupied leading cell is moved to the newly
vacated trailing cell. This guarantees no new water/solid overlap, exact
cell-count displacement and conservative total water fraction up to floating
point summation. It also keeps disjoint solid runs separate.

**Critical limitation:** The instantaneous leading-to-trailing remap is an
artificial through-body transfer. It is not local face-based advection, not
a physical swept-volume boundary condition, not a MAC velocity transfer, and
not a conservative physical fluid impulse or energy model. It is an occupancy
and scalar-mass bookkeeping kernel only. It is deliberately not wired into
LaggedKinematicPressureBody3D or the live CoupledMacWater3D solver.

## Reproducible evidence

- MSVC Release CMake/Ninja build of the new native test target succeeded.
- Full regression: **52/52 CTest suites PASS**, log
  ENGINE TEST STUBE/runs/p2_voxel_sweep_ctest_20261027.txt.
- Native test tests/p2_voxel_sweep_remap3d_tests.cpp:
  **76,222 assertions PASS**; 8^3, 12^3 and 16^3 grids, all three
  axes and both signs (18 cases); mass invariance, bounds, exact occupancy,
  displacement volume, reverse-mask recovery, and invalid-input rejection.
- Independent Python reconstruction of each of the 18 scenario's leading
  swept cells and expected displaced VOF, plus SHA-256 evidence:
  scripts/p2_voxel_sweep_acceptance.py ->
  ENGINE TEST STUBE/runs/p2_voxel_sweep_partial.json.
- P1 acceptance was re-executed after the CMake addition.
- The full phase gate still reports P0/P1 PASS and P2-P8 PARTIAL.

## Next P2 work

Replace the artificial through-body scalar transfer with a face-local
conservative swept-volume/VOF flux and explicit boundary velocity treatment;
integrate dynamic occupancy into the MAC projection and body predictor with
a genuine opposite fluid impulse and full pressure/viscous work accounting.
Validate on independent 3D experimental measurements and repeat UE5.8 native
Game/PIE E2E. None of these criteria is satisfied by this milestone.
