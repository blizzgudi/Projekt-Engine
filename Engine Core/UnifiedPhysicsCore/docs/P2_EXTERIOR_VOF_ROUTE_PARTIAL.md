# P2 PARTIAL - Exterior-only face-connected VOF route (27.10.2026)

**Full phase status: P2 PARTIAL.** This is a bounded topological mass
bookkeeping improvement, not a CFD or two-way FSI acceptance.

## Change and scope

`include/upc/p2_exterior_vof_route3d.hpp` provides an optional alternative
to the older `VoxelSweepRemap3D` through-body transfer. A rigid voxel mask
is translated by exactly one signed axis cell. The source of displaced water
is each newly occupied leading cell; the paired destination is the vacated
trailing cell of the same old contiguous body run.

For each **wet** leading cell, breadth-first search selects a shortest
six-neighbor path through cells free of both old and new moving-body
occupancy and all fixed obstacles. Direct leading-to-trailing transfer is
explicitly forbidden; a route must pass through at least one exterior
cell. Existing water fractions are shifted along the path toward the
vacated cell. This is a permutation of scalar fractions, preserving their
bounds and global sum. Dry motion needs no route and can succeed in a
sealed dry cavity. If a wet cell has no exterior route, the call fails
atomically without changing any caller-owned arrays.

The returned path indices make the transport topology auditable. This is
**not** a time-integrated VOF face flux: the entire exterior path is
permuted in one step, with no local pressure/velocity, no CFL, no fluid
impulse, no energy closure and no free-surface stress treatment. The
prototype has an explicit two-million-cell input limit and performs
one breadth-first search per wet entering cell. It is not wired into the
live MAC solver or UE5.8.

## Evidence

- MSVC Release CMake/Ninja build and new CTest target
  `physics_p2_exterior_vof_face_route_partial`.
- Full regression **53/53 CTest PASS**, stored under
  `ENGINE TEST STUBE/runs/p2_exterior_vof_route_ctest_20261027.txt`.
- Native test: 8^3, 12^3, 16^3, all 3 axes and both signs (18 cases);
  shortest three-face exterior detour for a single voxel; exact
  occupancy, scalar permutation, bounded VOF, zero volume residual,
  two disjoint bodies, blocked wet path, and allowed sealed dry sweep.
- Independent Python BFS reconstructs shortest path lengths and source
  fractions, verifies all 18 native CSV cases, validates SHA-256
  fingerprints and invokes the full phase gate.
  `scripts/p2_exterior_vof_route_acceptance.py` ->
  `ENGINE TEST STUBE/runs/p2_exterior_vof_route_partial.json`.
- P0/P1 remain PASS; P2-P8 remain PARTIAL. No UE Native-E2E is claimed.

## Next technical work

Replace the instantaneous scalar path permutation with a truly local
face-flux VOF transport constrained by a moving no-penetration boundary,
then couple MAC pressure projection, body impulse and mechanical energy.
Verify independent 3D CFD/FSI benchmarks and UE5.8 Native-E2E before
considering a full-phase acceptance.
