# P2 partial: real 6.00 s MARIN/SPHERIC Test 02, coarse native 3D MAC-PCG

Date: 2026-11-01. **P2 remains INCOMPLETE.** This is a real native
experiment-length trajectory and a measured-data **failure**, not a
validated two-phase CFD solver.

## What changed

- Raised the explicit MARIN native benchmark step guard from 300 to 600
  while retaining rejection of 601 steps. No global solver time-step or
  pressure-physics behavior was modified.
- Rebuilt the benchmark with native Win64 MSVC and executed **600 genuine
  consecutive 0.01 s steps** of `CoupledMacWater3D`, opt-in conservative
  throughflow, PCG pressure projection and actual MAC wall-tangential
  pressure interpolation. 601 state rows from t=0.00 through t=6.00 s.
- Real 32 x 10 x 10 3D grid at dx=0.10 m, represented tank
  3.20 x 1.00 x 1.00 m (experiment 3.22 m length), binary-cell obstacle
  at nominal x=0.6635..0.8245 m and original MARIN H1-H4/P1-P8
  locations. Original SPHERIC Test-02 measured time range extends to
  7.394 s, so no extrapolation is required.
- All 600 comparison timestamps for each of 12 channels are interpolated
  from the *original laboratory time series*, never synthesized.
- Numerically, maximum water-volume drift is
  **2.1316282072803006e-14 m3** and maximum projected wet divergence
  is **5.71426414866516e-09 s^-1**. These do not validate physical
  accuracy of free-surface stresses or pressure impulses.

## Actual 6.00 s laboratory comparison: FAIL

| Channel | Full 600-sample RMSE | Unit |
|---|---:|---|
| H1 | 0.13652547 | m |
| H2 | 0.091500164 | m |
| H3 | 0.14824530 | m |
| H4 | 0.22439875 | m |
| P1 | 3802.8123 | Pa |
| P2 | 4089.0611 | Pa |
| P3 | 3906.4808 | Pa |
| P4 | 3854.1361 | Pa |
| P5 | 3358.1434 | Pa |
| P6 | 3327.0660 | Pa |
| P7 | 3251.8372 | Pa |
| P8 | 3337.4079 | Pa |

**All four H1-H4 channels fail the predeclared 0.05 m RMSE criterion.**
Pressure channels are currently approximate tangential wall-pressure
proxies, not independently validated stress/force transducer footprints.
No pressure-validation threshold was silently invented. In particular,
a converged PCG residual is **not** equivalent to pressure measurements
converging with mesh refinement.

## Reproducible positive and negative evidence

- `scripts/p2_marin_pcg_build.cmd` rebuilds the native benchmark under
  the existing `build/` directory. Native command:
  `build/p2_marin_dambreak_heights.exe 600 throughflow dense interp pcg`.
- `python scripts/p2_marin_six_second_partial.py --run` performs the
  real native 6-s run, saves the complete output and audits it. Without
  `--run` it only verifies existing data, avoiding expensive reruns.
- `python tests/p2_marin_six_second_regression.py` passed **6/6**
  tests, including 6 targeted malformed-trace rejections, complete
  601-state trajectory, 12-channel/hash checks and the native 601-step
  invalid-budget rejection. The first audit caught a defect in the
  nonfinite-value corruption fixture; it was corrected and retested.
- MSVC CTest Release suite: **117/117 PASS**. This is code regression
  coverage, not experimental CFD acceptance.
- The first full 600-step native run succeeded, but its initial audit
  failed due to that independent corruption-test fixture; the fixture
  was fixed and the *same saved real native trace* was reaudited.
  No second expensive fluid run was necessary.

Pinned artifact hashes (SHA-256):

| Artifact | SHA-256 |
|---|---|
| Official experiment ZIP | `a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1` |
| `ENGINE TEST STUBE/runs/p2_marin_pcg_coarse600_dense.txt` | `b250ecaa0c72fe26aff19cea6821a6e617adb5ad26311cc05cb64a6f8e6664d6` |
| `build/p2_marin_dambreak_heights.exe` | `e49f9174751bf6a2ebab879cce4c278ac7831ed95ba72fdc19dd7c6549cb9413` |
| `ENGINE TEST STUBE/runs/p2_marin_6s_ctest.txt` | `2a1fe665d7f89dc09dc70da473ad813485fdeeb7794cdb1c6c6471c74ac2cfb5` |
| `ENGINE TEST STUBE/runs/p2_marin_6s_regression.txt` | `a04330e6dae499aaded6827c8451432a9e6ec7e1a8dc9c551ebfb9e73a977c42` |
| `ENGINE TEST STUBE/runs/p2_marin_pcg_6s_partial.json` | `192e198e4e72fbca285c8eeb7b513f74ecdb4432b7705d837f71b8b0f91f2f08` |

The JSON also fingerprints all tested source files, native binary and
laboratory CSV. Raw trace and logs remain under the existing
`ENGINE TEST STUBE/runs/` path. The original experiment files stay
under the existing `references/` directory and are not committed.

## What still blocks P2 full acceptance

The second independent >=6 s fine-grid experiment and a third grid
for convergence are not done. Geometric wall-sensor footprint,
stress-consistent water/air interface and traction, bidirectional
moving-solid fluid impulse, and global physical mass/momentum/energy
closure remain unvalidated. The current 3.20 m represented domain,
binary obstacle and under-resolved pressure probes are known model
errors. P2 must stay PARTIAL in `scripts/phase_gate.py` even with
117/117 CTest and successful native execution. No new Unreal 5.8
Native-E2E was performed in this increment.
