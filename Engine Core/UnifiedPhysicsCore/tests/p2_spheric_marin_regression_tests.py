r"""Regression and negative-control tests for the bounded SPHERIC/MARIN P2 study.

No original experimental data is modified. Temporary corruption fixtures are
created only under D:\Projekt Engine\ENGINE TEST STUBE\runs.
"""
from __future__ import annotations
import json
from pathlib import Path
import sys
import tempfile
import unittest

CORE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CORE/"scripts"))
import p2_spheric_experimental_ingest as ingest
import p2_spheric_marin_compare as compare

class SphericMarinRegressionTests(unittest.TestCase):
    def test_pinned_original_archive_and_workbook(self):
        rows=ingest.read_measurements()
        self.assertEqual(len(rows),7395)
        self.assertEqual(len(rows[0]),13)
        self.assertAlmostEqual(rows[0][-1],.55,delta=.01)

    def test_interpolation_at_known_experimental_point(self):
        rows=ingest.read_measurements()
        vals=ingest.interpolate(rows,.5)
        self.assertAlmostEqual(vals[-1],.4261492754253788,delta=1e-11)
        self.assertAlmostEqual(vals[8],.09480478262761362,delta=1e-11)

    def test_experimental_interpolation_rejects_extrapolation(self):
        rows=ingest.read_measurements()
        with self.assertRaises(ValueError):
            ingest.interpolate(rows,-.01)
        with self.assertRaises(ValueError):
            ingest.interpolate(rows,8.0)

    def test_native_geometry_probes_and_mass(self):
        exp,sim,report=compare.load()
        self.assertEqual(len(exp),7395)
        self.assertGreaterEqual(len(sim),7)
        self.assertEqual(len(report["channels"]),13)
        self.assertAlmostEqual(sim[0]["water_volume_m3"],.6644,delta=1e-11)
        self.assertLess(max(abs(row["water_volume_m3"]-.6644) for row in sim),1e-8)
        self.assertEqual(sim[-1]["time_s"],2.0)

    def test_real_measurements_reject_physics_claim(self):
        report=json.loads((compare.RUNS/"p2_spheric_marin_cfd_comparison_fail.json").read_text(encoding="utf-8"))
        self.assertTrue(report["benchmark_executed"])
        self.assertFalse(report["experimental_cfd_validated"])
        self.assertFalse(report["phase_complete"])
        self.assertFalse(report["heights_meet_rmse_target"])
        self.assertGreaterEqual(len(report["validation_fail_reasons"]),6)
        self.assertTrue(all(report["metrics"][h]["rmse_m"]>.05 for h in compare.HEIGHTS))

    def test_tampered_experimental_csv_rejected(self):
        with tempfile.TemporaryDirectory(prefix="marin_corrupt_",dir=compare.RUNS) as folder:
            original=compare.MEASURED
            corrupt=Path(folder)/"measured.csv"
            data=bytearray(original.read_bytes())
            data[-4]=ord("9") if data[-4]!=ord("9") else ord("8")
            corrupt.write_bytes(data)
            try:
                compare.MEASURED=corrupt
                with self.assertRaisesRegex(RuntimeError,"CSV SHA256"):
                    compare.load()
            finally:
                compare.MEASURED=original

    def test_tampered_native_geometry_rejected(self):
        with tempfile.TemporaryDirectory(prefix="marin_corrupt_",dir=compare.RUNS) as folder:
            original=compare.NATIVE
            corrupt=Path(folder)/"native.txt"
            data=original.read_bytes()
            self.assertIn(b"obstacle_x_m=[0.6635,0.8245]",data)
            corrupt.write_bytes(data.replace(b"obstacle_x_m=[0.6635,0.8245]",
                                             b"obstacle_x_m=[0.6735,0.8245]"))
            try:
                compare.NATIVE=corrupt
                with self.assertRaisesRegex(RuntimeError,"geometry"):
                    compare.load()
            finally:
                compare.NATIVE=original

    def test_tampered_source_archive_rejected(self):
        with tempfile.TemporaryDirectory(prefix="marin_corrupt_",dir=compare.RUNS) as folder:
            original=ingest.SOURCE
            corrupt=Path(folder)/"source.zip"
            data=bytearray(original.read_bytes())
            data[100]^=1
            corrupt.write_bytes(data)
            try:
                ingest.SOURCE=corrupt
                with self.assertRaisesRegex(RuntimeError,"SHA256 mismatch"):
                    ingest.read_measurements()
            finally:
                ingest.SOURCE=original

    def test_missing_native_log_rejected(self):
        original=compare.NATIVE
        try:
            compare.NATIVE=compare.RUNS/"marin_intentionally_missing_native_log.txt"
            with self.assertRaises(FileNotFoundError):
                compare.load()
        finally:
            compare.NATIVE=original

if __name__=="__main__":
    unittest.main(verbosity=2)
