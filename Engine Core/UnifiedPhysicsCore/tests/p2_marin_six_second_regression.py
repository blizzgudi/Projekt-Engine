"""Independent fail-closed checks for the real P2 6-second MARIN native trace."""
import json
import subprocess
import sys
import unittest
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CORE / "scripts"))
import p2_marin_six_second_partial as audit


class MarinSixSecondRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = audit.TRACE.read_text(encoding="utf-8")
        cls.rows = audit.parse_native(cls.raw)

    def test_full_600_steps_and_actual_time(self):
        self.assertEqual(len(self.rows), 601)
        self.assertAlmostEqual(self.rows[-1]["time_s"], 6., places=12)
        self.assertAlmostEqual(self.rows[0]["time_s"], 0., places=12)

    def test_six_corruption_cases_rejected(self):
        result = audit.regression_negatives(self.raw)
        self.assertEqual(len(result), 6)
        self.assertTrue(all(result.values()), result)

    def test_mass_and_projection(self):
        v = self.rows[0]["water_volume_m3"]
        self.assertLess(max(abs(row["water_volume_m3"]-v)
                            for row in self.rows), 1e-8)
        self.assertLess(max(row["max_projection_divergence_s_inv"]
                            for row in self.rows), 1e-6)

    def test_real_source_hash_and_all_twelve_metrics(self):
        report = json.loads(audit.REPORT.read_text(encoding="utf-8"))
        self.assertEqual(report["native_trace_sha256"], audit.sha(audit.TRACE))
        self.assertEqual(report["native_binary_sha256"], audit.sha(audit.BINARY))
        self.assertEqual(report["experiment_csv_sha256"],
                         audit.sha(audit.reference.MEASURED))
        for name, digest in report["source_sha256"].items():
            self.assertEqual(digest, audit.sha(CORE/name))
        self.assertEqual(set(report["metrics"]), set(audit.CHANNELS))
        self.assertTrue(all(m["samples"] == 600
                            for m in report["metrics"].values()))

    def test_fail_closed_full_phase(self):
        report = json.loads(audit.REPORT.read_text(encoding="utf-8"))
        self.assertIs(report["phase_complete"], False)
        self.assertIs(report["height_rmse_threshold_met"], False)
        self.assertIs(report["pressure_experimental_validation"], False)
        self.assertIs(report["two_grid_full_6s_completed"], False)

    def test_native_step_budget_rejects_601(self):
        p = subprocess.run(
            [str(audit.BINARY), "601", "throughflow", "dense", "interp", "pcg"],
            cwd=CORE, capture_output=True, text=True, timeout=15)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Invalid benchmark step budget", p.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
