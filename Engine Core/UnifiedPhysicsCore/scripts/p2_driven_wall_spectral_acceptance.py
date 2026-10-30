"""P2 partial: native 3D wall-driven shear versus independent discrete spectral sum."""
from __future__ import annotations
import hashlib
import json
import math
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

CORE = Path(__file__).resolve().parent.parent
LAB = CORE.parents[1] / "ENGINE TEST STUBE"
RUNS = LAB / "runs"
OUT = RUNS / "p2_driven_wall_spectral_partial.json"
LOG = RUNS / "p2_driven_wall_spectral_full_ctest_20261030.txt"
CTEST = Path(
    r"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7"
    r"\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
)
NATIVE = CORE / "build" / "upc_p2_driven_wall_spectral_tests.exe"
FILES = [
    "include/upc/mac3d.hpp",
    "include/upc/viscous_mac3d.hpp",
    "tests/p2_driven_wall_spectral3d_tests.cpp",
    "scripts/p2_driven_wall_spectral_full_build.cmd",
    "scripts/p2_driven_wall_spectral_acceptance.py",
    "docs/P2_DRIVEN_WALL_SPECTRAL_PARTIAL.md",
    "docs/PHASENPLAN.md",
    "scripts/phase_gate.py",
]

def independent_center(n: int, L: int, iterations: int = 40) -> float:
    """Orthogonal modal expansion of 3D Dirichlet/Neumann forced diffusion."""
    pi = math.pi
    alpha = 0.012 * 0.03 / (0.1 * 0.1)
    center = 0.0
    for mx in range(L):
        tx = (2 * mx + 1) * pi / (2 * L + 1)
        xn = sum(math.sin((i + 1) * tx) ** 2 for i in range(L))
        for my in range(1, n):
            ty = my * pi / n
            yn = sum(math.sin(y * ty) ** 2 for y in range(1, n))
            ys = sum(math.sin(y * ty) for y in range(1, n))
            for mz in range(1, n + 1):
                tz = mz * pi / (n + 1)
                zn = sum(math.sin((z + 1) * tz) ** 2 for z in range(n))
                zs = sum(math.sin((z + 1) * tz) for z in range(n))
                r = 1 + alpha * 2 * (
                    math.cos(tx) + math.cos(ty) + math.cos(tz) - 3
                )
                if not 0 < r < 1:
                    raise ValueError("Invalid spectral stability")
                projection = math.sin(tx) * ys * zs / (xn * yn * zn)
                gain = alpha * 0.4 * projection * (1 - r ** iterations) / (1 - r)
                center += gain * math.sin(tx) * math.sin((n // 2) * ty) * (
                    math.sin((n // 2 + 1) * tz)
                )
    return center

def main() -> None:
    if not CTEST.is_file() or not NATIVE.is_file():
        raise SystemExit("Missing CTest or native MSVC executable; run build cmd")
    native = subprocess.run([str(NATIVE)], cwd=CORE, text=True,
                            capture_output=True, timeout=60, check=False)
    ctest = subprocess.run([str(CTEST), "--test-dir", str(CORE / "build"),
                            "-C", "Release", "--parallel", "4",
                            "--output-on-failure"], cwd=CORE, text=True,
                           capture_output=True, timeout=180, check=False)
    RUNS.mkdir(parents=True, exist_ok=True)
    LOG.write_text(ctest.stdout + ctest.stderr, encoding="utf-8")
    text = native.stdout
    def numeric(name: str) -> float:
        match = re.search(r"\b" + re.escape(name) + r"=([0-9.eE+\-]+)", text)
        return float(match.group(1)) if match else float("nan")
    checks = []
    def add(name: str, value: object, passed: bool) -> None:
        checks.append({"name": name, "value": value, "pass": bool(passed)})
    add("native_exit", native.returncode, native.returncode == 0)
    count = numeric("checks")
    add("native_assertions", count, count >= 11000)
    add("40_stable_explicit_steps", numeric("steps"), numeric("steps") == 40)
    add("explicit_CFL", numeric("alpha"), abs(numeric("alpha") - 0.036) < 1e-14)
    for n, L in [(8, 5), (12, 7)]:
        measured = numeric(f"n{n}_center")
        reference = numeric(f"n{n}_reference")
        expected = independent_center(n, L)
        add(f"independent_3d_spectral_center_n{n}",
            {"actual": measured, "python": expected},
            math.isfinite(measured) and abs(measured - expected) < 3e-12)
        add(f"native_spectral_parity_n{n}",
            {"native": reference, "python": expected},
            math.isfinite(reference) and abs(reference - expected) < 3e-12)
        error = numeric(f"n{n}_error")
        add(f"all_3d_faces_n{n}", error, math.isfinite(error) and error < 3e-11)
        energy = numeric(f"n{n}_energy")
        add(f"positive_face_energy_n{n}", energy,
            math.isfinite(energy) and 0 < energy < 0.01)
    suite = re.search(r"100% tests passed, 0 tests failed out of (\d+)",
                      ctest.stdout)
    suite_count = int(suite.group(1)) if suite else 0
    add("full_ctest_exit", ctest.returncode, ctest.returncode == 0)
    add("existing_ctest_count", suite_count, suite_count == 87)
    add("source_fingerprints", len(FILES),
        len(FILES) == 8 and all((CORE / f).is_file() for f in FILES))
    hashes = {f: hashlib.sha256((CORE / f).read_bytes()).hexdigest()
              for f in FILES if (CORE / f).is_file()}
    accepted = all(c["pass"] for c in checks)
    report = {
        "phase": "P2",
        "milestone": "transient_3d_driven_wall_discrete_spectral_partial",
        "partial_accepted": accepted,
        "phase_complete": False,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "native_stdout": native.stdout.strip(),
        "native_stderr": native.stderr[-2000:],
        "ctest_count": suite_count,
        "ctest_sha256": hashlib.sha256(LOG.read_bytes()).hexdigest(),
        "source_sha256": hashes,
        "checks": checks,
        "limitations": (
            "Prescribed tangential ghost on fixed solid voxels, not moving "
            "body geometry or momentum reaction. Discrete diffusion-only "
            "spectral reference, not measured 3D Navier-Stokes/FSI, "
            "global physical energy closure, or UE5.8 Native-E2E."
        ),
    }
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False),
                   encoding="utf-8")
    print("P2_DRIVEN_WALL_SPECTRAL_PARTIAL=" + str(accepted))
    print("QUANTITATIVE_CHECKS=" + str(len(checks)))
    print("NATIVE_ASSERTIONS=" + str(int(count)) if math.isfinite(count)
          else "NATIVE_ASSERTIONS=missing")
    print("CTEST_SUITES=" + str(suite_count))
    print("REPORT=" + str(OUT))
    if not accepted:
        raise SystemExit("P2 3D driven wall spectral partial gate failed")

if __name__ == "__main__":
    main()
