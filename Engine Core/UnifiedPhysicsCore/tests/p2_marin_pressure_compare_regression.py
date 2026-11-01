"""P2 PARTIAL: positive/negative native experimental P1-P8 readback checks."""
from __future__ import annotations
import json
from pathlib import Path
import sys
import tempfile
import unittest

CORE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CORE/"scripts"))
import p2_marin_pressure_compare as compare

class P2MarinPressureBenchTests(unittest.TestCase):
    def test_real_native_pressure_eight_gauges(self):
        rows=compare.load_native()
        self.assertEqual(len(rows),201)
        self.assertEqual(rows[-1]["time_s"],2.0)
        self.assertEqual(len(compare.GAUGES),8)
        self.assertEqual(int(rows[-1]["pressure_probe_wet_count"]),8)
        fine=compare.load_native(compare.OBS_FINE,grid="fine")
        self.assertEqual(len(fine),201)
        self.assertEqual(fine[-1]["time_s"],2.0)
        self.assertEqual(int(fine[-1]["pressure_probe_wet_count"]),8)

    def test_original_experiment_not_claimed_to_pass(self):
        r=compare.compare()
        self.assertFalse(r["physical_pressure_validation"])
        self.assertFalse(r["stress_consistent_wall_pressure"])
        self.assertFalse(r["phase_complete"])
        self.assertFalse(r["full_3d_fluid_pressure_abnahme"])
        self.assertTrue(all(r["sensor_aliases_from_unresolved_coarse_grid"].values()))
        self.assertEqual(set(r["metrics_pa"]),set(compare.GAUGES))
        self.assertEqual(set(r["fine_metrics_pa"]),set(compare.GAUGES))
        self.assertTrue(all(r["sensor_aliases_from_unresolved_fine_grid"].values()))
        self.assertEqual(sum(r["fine_versus_coarse_rmse_improved"].values()),0)
        self.assertTrue(all(v["rmse_pa"]>5000 for v in r["fine_metrics_pa"].values()))
        self.assertTrue(all(v["sample_count"]==200 for v in r["metrics_pa"].values()))

    def _corrupt(self,operation):
        original=compare.OBS.read_bytes()
        text=original.decode("utf-16" if original.startswith(bytes((255,254))) else "utf-8-sig")
        after=operation(text.replace("\r\n","\n"))
        self.assertNotEqual(after,text)
        with tempfile.TemporaryDirectory(
            prefix="p2_marin_pressure_negative_",dir=compare.RUNS
        ) as folder:
            path=Path(folder)/"changed_pressure.txt"
            path.write_text(after,encoding="utf-8")
            return compare.load_native(path)

    def test_fine_trace_cannot_pass_using_coarse_grid_tag(self):
        with self.assertRaisesRegex(ValueError,"geometry/proxy"):
            compare.load_native(compare.OBS_FINE,grid="coarse")

    def test_pressure_mismatch_worsens_on_fine_grid(self):
        r=compare.compare()
        for name in compare.GAUGES:
            self.assertGreater(r["fine_metrics_pa"][name]["rmse_pa"],
                               r["metrics_pa"][name]["rmse_pa"])

    def test_metadata_missing_proxy_provenance_rejected(self):
        with self.assertRaisesRegex(ValueError,"geometry/proxy"):
            self._corrupt(lambda text:text.replace(
                "pressure_proxy_only=true","pressure_proxy_only=false"))

    def test_unregistered_second_pressure_field_rejected(self):
        with self.assertRaisesRegex(ValueError,"column order"):
            self._corrupt(lambda text:text.replace("P1_Pa,P2_Pa","P9_Pa,P2_Pa"))

    def test_native_failure_cannot_be_pass(self):
        with self.assertRaisesRegex(ValueError,"not successful"):
            self._corrupt(lambda text:text.replace(
                "PASS MARIN_COARSE_BASELINE","FAIL MARIN_COARSE_BASELINE"))

    def test_missing_timestamp_is_rejected(self):
        def corrupt(text):
            lines=text.splitlines()
            return "\n".join(lines[:11]+lines[12:])+"\n"
        with self.assertRaisesRegex(ValueError,"length/header"):
            self._corrupt(corrupt)

    def test_pressure_nan_rejected(self):
        def corrupt(text):
            lines=text.splitlines()
            fields=lines[12].split(",")
            fields[7]="nan"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"Nonfinite"):
            self._corrupt(corrupt)

    def test_wet_sensor_count_out_of_range_rejected(self):
        def corrupt(text):
            lines=text.splitlines()
            fields=lines[12].split(",")
            fields[-1]="9"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"wet gauge"):
            self._corrupt(corrupt)

    def test_pressure_multiplicative_forgery_rejected(self):
        def corrupt(text):
            lines=text.splitlines()
            fields=lines[12].split(",")
            fields[7]="999999999"
            lines[12]=",".join(fields)
            return "\n".join(lines)+"\n"
        with self.assertRaisesRegex(ValueError,"numeric bound"):
            self._corrupt(corrupt)

if __name__=="__main__":
    unittest.main(verbosity=2)
