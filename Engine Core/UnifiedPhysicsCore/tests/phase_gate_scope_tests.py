"""Regression guard: green MVP reports cannot silently close full P2-P8."""
import importlib.util
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"scripts"/"phase_gate.py"
spec=importlib.util.spec_from_file_location("phase_gate",p)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
phases={f"P{i}":{"status":"PASS","evidence":"synthetic green MVP"}
        for i in range(9)}
out=m.enforce_full_phase_scope(phases)
assert out["P0"]["status"]=="PASS"
assert out["P1"]["status"]=="PASS"
for i in range(2,9):
    p=out[f"P{i}"]
    assert p["status"]=="PARTIAL",i
    assert p["prototype_status"]=="PASS",i
    assert p["completion_scope"]=="bounded_prototype",i
    assert p["full_acceptance_open"],i
assert not all(p["status"]=="PASS" for p in out.values())
print("PASS: all green P0-P8 prototype reports cannot close full P2-P8")
