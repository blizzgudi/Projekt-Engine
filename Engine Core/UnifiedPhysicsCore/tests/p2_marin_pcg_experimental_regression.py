"""P2 PARTIAL: real 3D MAC-PCG vs Gauss-Seidel and original MARIN data.

A green regression verifies native execution and measurable numerical
improvement, NEVER an experimentally accepted 3D fluid/FSI phase.
"""
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path

CORE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CORE/"scripts"))
import p2_marin_pcg_compare as audit


class P2PcgMeasuredNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.report=audit.evaluate(require_fine=True)

    def test_real_two_grid_time_series_and_mass_conservation(self):
        self.assertEqual(set(self.report["grids"]),{"coarse","fine"})
        for name in ("coarse","fine"):
            row=self.report["grids"][name]
            self.assertEqual(row["201_native_time_states"],201)
            self.assertLess(row["maximum_volume_drift_m3"],1e-8)
            self.assertLess(row["maximum_real_wet_projection_divergence_s_inv"],1e-6)
            self.assertEqual(len(row["metrics"]),12)

    def test_pressure_improves_against_legacy_both_grids(self):
        for name in ("coarse","fine"):
            metrics=self.report["grids"][name]["metrics"]
            self.assertTrue(all(metrics[f"P{i}_Pa"]["rmse_improved"]
                                for i in range(1,9)))

    def test_spatial_refinement_improves_all_eight_pressure_rmse(self):
        for i in range(1,9):
            name=f"P{i}_Pa"
            self.assertLess(
                self.report["grids"]["fine"]["metrics"][name]["rmse_pcg"],
                self.report["grids"]["coarse"]["metrics"][name]["rmse_pcg"])

    def test_pressure_accuracy_not_good_enough_for_physical_validation(self):
        self.assertFalse(self.report["real_pressure_physically_validated"])
        self.assertFalse(self.report["real_water_height_physically_validated"])
        self.assertFalse(self.report["complete_3d_fluid_phase"])
        self.assertFalse(self.report["phase_complete"])
        self.assertFalse(self.report["full_6_second_record_completed"])
        self.assertTrue(all(
            self.report["grids"]["fine"]["metrics"][f"H{i}_m"]["rmse_pcg"]>.05
            for i in range(1,5)))
        self.assertTrue(all(
            self.report["grids"]["fine"]["metrics"][f"P{i}_Pa"]["rmse_pcg"]>1000
            for i in range(1,9)))

    def _corrupt(self,grid:str,edit):
        source=audit.COARSE if grid=="coarse" else audit.FINE
        raw=source.read_bytes()
        data=raw.decode(
            "utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
        modified=edit(data.replace("\r\n","\n"))
        self.assertNotEqual(data.replace("\r\n","\n"),modified)
        with tempfile.TemporaryDirectory(
            prefix="p2_pcg_measured_negative_",
            dir=audit.RUNS
        ) as temp:
            path=Path(temp)/"forged_native.txt"
            path.write_text(modified,encoding="utf-8")
            return audit.load(path,grid)

    def test_no_false_pcg_tag(self):
        with self.assertRaisesRegex(RuntimeError,"provenance"):
            self._corrupt("coarse",lambda s:s.replace(
                "pressure_solver=pcg","pressure_solver=gauss_seidel"))

    def test_missing_native_time_state_rejected(self):
        def drop(s):
            lines=s.splitlines()
            return "\n".join(lines[:40]+lines[41:])+"\n"
        with self.assertRaisesRegex(RuntimeError,"provenance or length"):
            self._corrupt("fine",drop)

    def test_failed_native_process_rejected(self):
        with self.assertRaises(ValueError):
            self._corrupt("coarse",lambda s:s.replace(
                "PASS MARIN_COARSE_BASELINE","FAIL MARIN_COARSE_BASELINE"))

    def test_mislabeled_3d_grid_rejected(self):
        with self.assertRaisesRegex(RuntimeError,"provenance"):
            self._corrupt("fine",lambda s:s.replace(
                " grid=fine sampling=dense"," grid=coarse sampling=dense"))

    def test_nan_pressure_cell_rejected(self):
        def inject(s):
            lines=s.splitlines()
            fields=lines[20].split(",")
            fields[7]="nan"
            lines[20]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"Nonfinite"):
            self._corrupt("coarse",inject)


if __name__=="__main__":
    unittest.main(verbosity=2)
