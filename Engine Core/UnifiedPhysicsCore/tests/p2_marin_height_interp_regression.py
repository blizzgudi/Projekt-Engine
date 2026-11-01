"""Independent fail-closed QA of the real 3D MAC H1-H4 interpolation trace."""
import math
import sys
import unittest
from pathlib import Path

CORE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(CORE / "scripts"))
import p2_marin_height_interp_partial_acceptance as audit


class HeightInterpolationRegression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.raw = audit.NEW.read_text(encoding="utf-8-sig")
        cls.new = audit.parse(cls.raw, True)
        cls.old = audit.parse(audit.OLD.read_text(encoding="utf-8-sig"), False)

    def test_real_201_native_states(self):
        self.assertEqual(len(self.new), 201)
        self.assertAlmostEqual(self.new[-1]["time_s"], 2, places=12)

    def test_reject_six_independent_corruptions(self):
        rejections = audit.mutations_rejected(self.raw)
        self.assertEqual(len(rejections), 6)
        self.assertTrue(all(rejections.values()), rejections)

    def test_unmodified_native_mac_pressure_volume_and_divergence(self):
        compare = audit.compare(self.new, self.old)
        self.assertTrue(all(v <= 1e-10 for v in
                            compare["max_solver_signal_differences"].values()))

    def test_subcell_height_is_not_legacy_nearest_column(self):
        compare = audit.compare(self.new, self.old)
        self.assertGreater(max(
            compare["max_height_signal_differences_m"].values()), 1e-8)

    def test_real_experiment_unchanged_and_window_covers_native(self):
        rows, _, source = audit.measured.load()
        self.assertEqual(source["source_zip_sha256"], audit.ZIP_SHA)
        self.assertGreaterEqual(len(rows), 7395)
        self.assertLessEqual(rows[0]["time_s"], self.new[1]["time_s"])
        self.assertGreaterEqual(rows[-1]["time_s"], 2)

    def test_no_silent_full_phase_acceptance(self):
        rows, _, _ = audit.measured.load()
        errors = {}
        for channel in audit.HEIGHTS:
            residuals = [r[channel]-audit.measured.lerp(
                rows, r["time_s"], channel) for r in self.new[1:]]
            errors[channel] = math.sqrt(sum(e*e for e in residuals)
                                        / len(residuals))
        self.assertTrue(any(e > .05 for e in errors.values()), errors)


if __name__ == "__main__":
    unittest.main(verbosity=2)
