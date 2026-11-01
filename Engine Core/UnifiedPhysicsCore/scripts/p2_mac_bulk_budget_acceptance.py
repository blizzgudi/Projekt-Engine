"""P2 PARTIAL: independent periodic constant-density MAC bulk SI budget gate."""
from __future__ import annotations
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
RUNS = CORE.parents[1] / "ENGINE TEST STUBE" / "runs"
PREFIX = "p2_mac_bulk_budget"
NATIVE = RUNS / (PREFIX + "_native_20261101.txt")
COMPILE = RUNS / (PREFIX + "_compile_20261101.txt")
CTEST = RUNS / (PREFIX + "_ctest_20261101.txt")
EXE = CORE / "build" / "upc_p2_mac_bulk_budget_tests.exe"
OUT = RUNS / (PREFIX + "_partial.json")
SOURCES = [
    "include/upc/p2_viscous_mac_plic_sequence3d.hpp",
    "include/upc/p2_viscous_nonlinear_mac_plic3d.hpp",
    "include/upc/p2_nonlinear_mac_plic3d.hpp",
    "tests/p2_mac_bulk_budget3d_tests.cpp",
    "scripts/p2_mac_bulk_budget_build.cmd",
    "scripts/p2_mac_bulk_budget_acceptance.py",
    "docs/P2_MAC_BULK_BUDGET_PARTIAL.md",
]

def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()

def main() -> None:
    native = NATIVE.read_text(encoding="utf-8")
    compile_log = COMPILE.read_text(encoding="utf-8")
    ctest = CTEST.read_text(encoding="utf-8")
    checks = []
    def check(name: str, value, ok: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(ok)})
    m = re.search(
        r"PASS: periodic MAC bulk budget assertions=(\d+) phase_complete=false",
        native)
    assertions = int(m.group(1)) if m else 0
    check("native_assertions", assertions, assertions >= 100)
    check("native_no_fail", "FAIL:" not in native, "FAIL:" not in native)
    check("msvc_compile", compile_log.strip(),
          bool(compile_log.strip()) and
          "error C" not in compile_log and "fatal error" not in compile_log.lower())
    check("full_ctest_regression", ctest.splitlines()[-1] if ctest else "",
          "100% tests passed" in ctest and
          "0 tests failed out of 106" in ctest)
    seen = set()
    references = []
    for line in native.splitlines():
        p = line.split()
        if not p or p[0] != "FOURIER":
            continue
        if len(p) != 7:
            raise AssertionError("Malformed Fourier output")
        n = int(p[1])
        if n not in (8,12) or n in seen:
            raise AssertionError("Unexpected Fourier grid")
        seen.add(n)
        momentum, initial_k, final_k, max_error, telescoping = map(float,p[2:])
        factor = 1.0
        for dt,nu in ((.2,.002),(.4,.004),(.15,0.0)):
            if nu > 0:
                alpha_total = nu*dt/(.1*.1)
                substeps = math.ceil(alpha_total*8-1e-14)
                factor *= (1-4*alpha_total/substeps*
                           math.sin(math.pi/n)**2)**substeps
        mass = n**3 * 1000*.1**3
        expected_p = mass*.02
        expected_initial = .5*mass*(.02**2+.5*.04**2)
        expected_final = .5*mass*(.02**2+.5*.04**2*factor**2)
        check(f"momentum_{n}",momentum,abs(momentum-expected_p)<3e-9)
        check(f"initial_kinetic_{n}",initial_k,
              abs(initial_k-expected_initial)<3e-9)
        check(f"final_kinetic_{n}",final_k,
              abs(final_k-expected_final)<3e-9)
        check(f"viscous_energy_decay_{n}",final_k,
              0 < final_k < initial_k)
        check(f"fourier_error_{n}",max_error,
              math.isfinite(max_error) and max_error<3e-9)
        check(f"telescoping_{n}",telescoping,abs(telescoping)<3e-9)
        references.append({"grid":n,"expected_momentum_kg_m_s":expected_p,
                           "expected_initial_kinetic_j":expected_initial,
                           "expected_final_kinetic_j":expected_final,
                           "measured_final_kinetic_j":final_k})
    check("both_3d_grids",sorted(seen),seen=={8,12})
    rows=[line.split() for line in native.splitlines()
          if line.startswith("MIXED ")]
    if len(rows)!=1 or len(rows[0])!=6:
        raise AssertionError("Missing mixed-water bulk audit")
    water0,water1,kin0,kin1,residual=map(float,rows[0][1:])
    check("mixed_water_mass_initial",water0,abs(water0-432)<1e-8)
    check("mixed_water_mass_final",water1,abs(water1-432)<1e-8)
    check("mixed_bulk_kinetic",kin1,0<kin1<kin0)
    check("mixed_telescoping",residual,abs(residual)<1e-8)
    check("eight_invalid_cases","NEGATIVE 8" in native,"NEGATIVE 8" in native)
    hashes={name:sha(CORE/name) for name in SOURCES}
    check("source_hashes",len(hashes),len(hashes)==7)
    accepted=all(c["pass"] for c in checks)
    report={
        "phase":"P2","milestone":"periodic_mac_bulk_momentum_kinetic_partial",
        "partial_accepted":accepted,"phase_complete":False,
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "native_assertions":assertions,"checks":checks,"reference":references,
        "source_sha256":hashes,
        "native_binary_sha256":sha(EXE),"native_log_sha256":sha(NATIVE),
        "compile_log_sha256":sha(COMPILE),"ctest_log_sha256":sha(CTEST),
        "limitations":[
            "Periodic constant-density full-domain MAC bulk quadrature; passive VOF",
            "Not water-only kinetic energy or a global two-phase energy law",
            "No moving-solid FSI, independent measured 3D CFD, or new UE5.8 Native E2E",
            "P2-P8 full phases remain incomplete",
        ],
    }
    OUT.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(f"P2_MAC_BULK_BUDGET_PARTIAL={accepted} "
          f"CHECKS={sum(x['pass'] for x in checks)}/{len(checks)} "
          f"NATIVE_ASSERTIONS={assertions} PHASE_COMPLETE=False")
    if not accepted:
        raise SystemExit(1)

if __name__=="__main__":
    main()
