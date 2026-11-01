"""P2 PARTIAL native SPHERIC/MARIN dense 3D grid-evidence regression.

Positive + negative controls. Original measurements and logs are read-only;
temporary manipulated copies stay under ENGINE TEST STUBE/runs.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

CORE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CORE/"scripts"))
import p2_spheric_marin_grid_refinement as grid


class GridRefinementEvidenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reference=grid.compare()
        cls.original=grid.LOGS.copy()

    @classmethod
    def tearDownClass(cls):
        grid.LOGS.clear()
        grid.LOGS.update(cls.original)

    def test_native_measured_dense_201_samples_on_each_grid(self):
        for name in ("coarse","fine"):
            rows=grid.load_native(grid.LOGS[name],name)
            self.assertEqual(len(rows),201)
            self.assertAlmostEqual(rows[0]["time_s"],0.0,delta=1e-12)
            self.assertAlmostEqual(rows[-1]["time_s"],2.0,delta=1e-12)

    def test_simulated_water_volume_remains_conserved(self):
        for name in ("coarse","fine"):
            record=self.reference["native_runs"][name]
            self.assertAlmostEqual(record["water_volume_initial_m3"],0.6644,delta=1e-9)
            self.assertLess(record["water_volume_max_drift_m3"],1e-8)

    def test_no_false_experimental_phase_acceptance(self):
        self.assertIs(self.reference["phase_complete"],False)
        self.assertIs(self.reference["experimentally_validated"],False)
        self.assertIs(self.reference["pressure_sensor_validation"],False)
        self.assertIs(self.reference["runtime_6seconds_validated"],False)
        self.assertIs(self.reference["full_grid_order_verified"],False)
        self.assertGreater(len(self.reference["unmet"]),0)

    def test_independent_height_metric_threshold_remains_fail(self):
        self.assertFalse(all(self.reference["fine_heights_within_0.05m_rmse"].values()))
        self.assertTrue(self.reference["all_heights_improved"])
        self.assertEqual(len(self.reference["heights_improve_with_refinement"]),4)

    def _with_tampered_log(self,which,transform):
        source=grid.LOGS[which]
        original_bytes=source.read_bytes()
        encoding="utf-16" if original_bytes.startswith(bytes((255,254))) else "utf-8-sig"
        original=original_bytes.decode(encoding)
        changed=transform(original)
        self.assertNotEqual(changed,original)
        with tempfile.TemporaryDirectory(prefix="p2_marin_grid_negative_",dir=grid.RUNS) as tmp:
            target=Path(tmp)/"fake.txt"
            target.write_text(changed,encoding="utf-8")
            return grid.load_native(target,which)

    def test_wrong_geometry_must_fail(self):
        with self.assertRaisesRegex(RuntimeError,"grid geometry"):
            self._with_tampered_log(
                "fine",lambda text:text.replace("nx=64 ny=20 nz=20","nx=32 ny=20 nz=20"))

    def test_forged_success_flag_must_fail(self):
        with self.assertRaisesRegex(RuntimeError,"actual PASS"):
            self._with_tampered_log(
                "coarse",lambda text:text.replace(
                    "PASS MARIN_COARSE_BASELINE","FAIL MARIN_COARSE_BASELINE"))

    def test_incomplete_trajectory_must_fail(self):
        def truncate(text):
            lines=text.splitlines()
            return "\n".join(lines[:-2]+[lines[-1]])+"\n"
        with self.assertRaisesRegex(RuntimeError,"Missing MARIN time samples"):
            self._with_tampered_log("fine",truncate)

    def test_non_monotone_time_must_fail(self):
        def corrupt(text):
            lines=text.splitlines()
            cols=lines[15].split(",")
            cols[0]="0.001"
            lines[15]=",".join(cols)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(RuntimeError,"Nonmonotone"):
            self._with_tampered_log("coarse",corrupt)


if __name__=="__main__":
    unittest.main(verbosity=2)
