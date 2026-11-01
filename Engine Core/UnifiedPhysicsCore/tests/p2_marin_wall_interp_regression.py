"""P2 PARTIAL: positive and deliberately corrupted MARIN wall-interpolation
native evidence. No acceptance of physical pressure agreement.
"""
from __future__ import annotations
import sys
import tempfile
from pathlib import Path
import unittest

CORE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CORE/"scripts"))
import p2_marin_wall_interp_compare as compare

class WallInterpRegression(unittest.TestCase):
    def test_two_true_native_3d_traces(self):
        for name,path in (("coarse",compare.COARSE),("fine",compare.FINE)):
            rows=compare.load(path,name)
            self.assertEqual(len(rows),201)
            self.assertEqual(rows[-1]["time_s"],2.0)
            self.assertEqual(int(rows[-1]["pressure_probe_wet_count"]),8)

    def test_official_measurement_mismatch_remains_visible(self):
        r=compare.evaluate()
        self.assertFalse(r["experimental_pressure_validation"])
        self.assertFalse(r["phase_complete"])
        self.assertFalse(r["normal_wall_stress_reconstructed"])
        self.assertFalse(r["independent_spatial_resolution_of_transducers"])
        self.assertEqual(len(r["grids"]["coarse"]["rmse_pa"]),8)
        self.assertEqual(len(r["grids"]["fine"]["rmse_pa"]),8)
        self.assertTrue(all(not v for v in
                            r["fine_vs_coarse_rmse_improved"].values()))
        self.assertTrue(all(r["grids"][g]["rmse_pa"][k]["rmse_pa"]>5000
                            for g in ("coarse","fine") for k in compare.GAUGES))

    def test_height_rmse_and_noninvasive_pressure_diagnostic(self):
        r=compare.evaluate()
        self.assertFalse(r["experimental_height_validation"])
        for g in ("coarse","fine"):
            self.assertEqual(set(r["grids"][g]["height_rmse_m"]),
                             {"H1_m","H2_m","H3_m","H4_m"})
            self.assertEqual(r["grids"][g]["historical_height_volume_max_difference"],0)
            self.assertTrue(all(v>0.05 for v in
                                r["grids"][g]["height_rmse_m"].values()))

    def test_tampered_height_invariant_rejected(self):
        actual=compare.load(compare.COARSE,"coarse")
        prior=compare.historical.load_native(compare.historical.OBS,grid="coarse")
        altered=[dict(row) for row in actual]
        altered[10]["H1_m"]+=.001
        with self.assertRaisesRegex(ValueError,"altered native fluid height"):
            compare.verify_height_invariant(altered,prior)

    def corrupt(self,change):
        raw=compare.COARSE.read_bytes()
        s=raw.decode("utf-16" if raw.startswith(bytes((255,254))) else "utf-8-sig")
        altered=change(s.replace("\r\n","\n"))
        self.assertNotEqual(s.replace("\r\n","\n"),altered)
        with tempfile.TemporaryDirectory(prefix="p2_wall_interp_neg_",
                                         dir=compare.RUNS) as d:
            path=Path(d)/"fake.txt"
            path.write_text(altered,encoding="utf-8")
            return compare.load(path,"coarse")

    def test_fine_grid_falsely_labeled_coarse_rejected(self):
        with self.assertRaisesRegex(ValueError,"provenance"):
            compare.load(compare.FINE,"coarse")

    def test_old_nearest_cell_not_interpolated_rejected(self):
        with self.assertRaisesRegex(ValueError,"provenance"):
            compare.load(compare.RUNS/"p2_marin_pressure_coarse200_dense.txt",
                         "coarse")

    def test_header_relabeling_rejected(self):
        with self.assertRaisesRegex(ValueError,"provenance"):
            self.corrupt(lambda s:s.replace("interpolated_tangent","true"))

    def test_failed_native_run_rejected(self):
        with self.assertRaisesRegex(ValueError,"not PASS"):
            self.corrupt(lambda s:s.replace("PASS MARIN_COARSE_BASELINE",
                                            "FAIL MARIN_COARSE_BASELINE"))

    def test_missing_row_rejected(self):
        def missing(s):
            lines=s.splitlines()
            return "\n".join(lines[:20]+lines[21:])+"\n"
        with self.assertRaisesRegex(ValueError,"length/header"):
            self.corrupt(missing)

    def test_wrong_timestamp_rejected(self):
        def time(s):
            lines=s.splitlines()
            fields=lines[12].split(",")
            fields[0]="9.5"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"time mismatch"):
            self.corrupt(time)

    def test_corrupted_channel_rejected(self):
        with self.assertRaisesRegex(ValueError,"columns"):
            self.corrupt(lambda s:s.replace("P1_Pa,P2_Pa","P0_Pa,P2_Pa"))

    def test_nan_pressure_rejected(self):
        def nan(s):
            lines=s.splitlines()
            fields=lines[12].split(",")
            fields[7]="nan"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"Nonfinite"):
            self.corrupt(nan)

    def test_impossible_wet_count_rejected(self):
        def wet(s):
            lines=s.splitlines()
            fields=lines[12].split(",")
            fields[-1]="9"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"wet count"):
            self.corrupt(wet)

    def test_mass_drift_rejected(self):
        def drift(s):
            lines=s.splitlines()
            fields=lines[12].split(",")
            fields[5]="123"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"volume drift"):
            self.corrupt(drift)

    def test_pressure_overflow_rejected(self):
        def huge(s):
            lines=s.splitlines()
            fields=lines[12].split(",")
            fields[7]="1e10"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"numeric overflow"):
            self.corrupt(huge)

if __name__=="__main__":
    unittest.main(verbosity=2)
