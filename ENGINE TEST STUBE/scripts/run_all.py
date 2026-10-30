"""All experiment entry points with auditable run records (stdlib only)."""
from datetime import datetime, timezone
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]

def main():
    spec = json.loads((ROOT / "lab.json").read_text(encoding="utf-8"))
    results = []
    for item in spec["experiments"]:
        exp = ROOT / item["path"]
        record = {"id": item["id"], "steps": []}
        for name, filename in (("unit_tests", item["test"]), ("simulation", item["simulation"]), ("benchmark", item.get("benchmark"))):
            if not filename:
                continue
            args = [sys.executable, "-m", "unittest", "-v", filename] if name == "unit_tests" else [sys.executable, filename]
            try:
                process = subprocess.run(args, cwd=exp, capture_output=True, text=True, timeout=120)
                record["steps"].append({"step": name, "exit_code": process.returncode, "passed": process.returncode == 0, "stdout": process.stdout[-12000:], "stderr": process.stderr[-12000:]})
            except Exception as exc:
                record["steps"].append({"step": name, "exit_code": None, "passed": False, "error": str(exc)})
        record["passed"] = all(step["passed"] for step in record["steps"])
        results.append(record)
        print(f"{item['id']}: {'PASS' if record['passed'] else 'FAIL'}", flush=True)
    runs = ROOT / "runs"
    runs.mkdir(exist_ok=True)
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    report = runs / f"run_{timestamp}.json"
    report.write_text(json.dumps({"project":spec["name"],"run_time_utc":timestamp,"python":sys.version,"results":results}, ensure_ascii=False, indent=2), encoding="utf-8")
    print("Report:", report, flush=True)
    return 0 if all(result["passed"] for result in results) else 1

if __name__ == "__main__":
    raise SystemExit(main())
