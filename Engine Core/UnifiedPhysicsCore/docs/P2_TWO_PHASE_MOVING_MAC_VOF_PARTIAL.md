# P2 partial: moving water/air VOF with variable-density MAC projection (02.11.2026)

## Real implemented scope

A new opt-in, transactional `p2_two_phase_mac_vof_step3d` connects the
existing **two-phase variable-density staggered MAC pressure projection** to
the existing **conservative 3D donor-cell VOF face flux**. Both water and air
carry a shared incompressible velocity and pressure field. Each cell has a
water volume fraction alpha in [0,1] and a mixture density

    rho = rho_air + (rho_water - rho_air) * alpha.

At each step the actual pressure-corrected MAC face velocity transports the
water fraction through all three axes, then recomputes the next mixture
density. The full-box water and air mass inventories are audited separately.
Projection, transport and density update commit only after all checks pass;
invalid input, nonconvergent PCG or outgoing VOF CFL leave the caller state
unchanged. The projected field obeys six impermeable boundaries.

This is a **real moving 3D phase fraction**, rather than the previous
static sharp water/air density mask. It is not yet a complete two-fluid
Navier-Stokes model: velocity is projected but **not advected by momentum
flux**, there is no capillary or stress-continuity update, no moving solid,
and no physically closed total kinetic-energy balance across VOF transport.
The pressure-step orthogonality ledger is valid only at the old density
before the transport. It must not be misrepresented as a full energy budget.

## Native reproduction and independent QA

Run only from `D:\Projekt Engine`:

    Engine Core\UnifiedPhysicsCore\scripts\p2_two_phase_mac_vof_build.cmd
    py -3 "Engine Core\UnifiedPhysicsCore\scripts\p2_two_phase_mac_vof_acceptance.py"

The MSVC Release native executable uses 4x4x4, 6x5x4 and 8x6x5 grids.
Each grid starts with a spatially varying water/air mixture, a manufactured
variable-density pressure gradient, and an independent **three-dimensional
discrete curl** with nonzero vertical velocity and all impermeable walls.
After pressure recovery the actual corrected velocity transports alpha for
eight successive steps. An independent cell-centric Python FV donor solver
computes the eight-step alpha field and verifies exact water/air mass,
specific cell fractions, vertical velocity and outgoing CFL. Native C++
also checks every cell's mixture density, every staggered velocity,
pressure recovery, conservation and divergence.

The negative native cases reject invalid alpha, inconsistent mixture
density, nonphysical material density, wall leakage, nonfinite velocity,
excessive VOF CFL and under-iterated PCG with no partial state commit.
Independent QA reruns the actual executable, rejects seven corrupted
native measurement fixtures, checks full CTest, official `phase_gate.py`,
and records SHA-256 hashes for source, binary, logs and the historical
negative MARIN measurement report.

The initial build exposed a **negative test-fixture bug**: direct vector
equality treats a deliberately injected NaN as different from itself,
falsely flagging an unchanged state. The test now compares NaNs
explicitly while still detecting any real mutation. This initial failed
native run remains in the local job history; it was **not** counted as
a successful test or phase acceptance.

## Explicit acceptance boundary

**P2 remains PARTIAL; full phase gate must remain open.** The historical
MARIN/SPHERIC full-6s H1-H4 RMSE threshold is not met, and P1-P8
pressure sensors and independent three-grid experimental convergence
are not accepted. The new path has no stress-consistent moving interface,
momentum self-advection, bidirectional moving-solid FSI, global physical
mass/impulse/energy closure or UE5.8 Native-E2E. The new manufactured
reference grids validate the algorithmic coupling only; they are not
independent experimental CFD convergence evidence.

## Verified native result

The corrected MSVC Release run returned exit code 0:
**19,038 native assertions and 136/136 CTest PASS**.
All three 3D moving-VOF grids passed eight successive steps:
4x4x4 / 6x5x4 / 8x6x5, with water masses
17.92 / 33.60 / 58.56 kg and air masses
0.055296 / 0.103680 / 0.217728 kg.
Largest measured per-step mass residuals were
1.42e-14 / 4.26e-14 / 9.95e-14 kg.
Maximum independent VOF-field errors were
3.33e-16 / 3.33e-16 / 7.77e-16, and
maximum pressure errors were
1.81e-12 / 1.82e-12 / 9.21e-12 Pa.
The maximum divergence after projection was
7.28e-14 / 3.76e-14 / 1.23e-13 s^-1.
The independent QA report must still be generated and must explicitly
keep phase_complete=false.
