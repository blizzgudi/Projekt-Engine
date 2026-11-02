r"""Independent 6s P2 MARIN/SPHERIC audit regression, including
deliberately corrupted native process output. No full CFD PASS is implied."""
import sys
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
import p2_marin_full6s_two_grid_audit as audit

class P2MarinTwoGridSixSeconds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.samples={}
        cls.texts={}
        for name,path in audit.LOGS.items():
            cls.texts[name]=path.read_text(encoding="utf-8")
            cls.samples[name]=audit.parse(cls.texts[name],name)

    def test_si_units_and_pinned_geometries(self):
        self.assertEqual(audit.SPEC["coarse"],(32,10,10,.1))
        self.assertEqual(audit.SPEC["fine"],(64,20,20,.05))
        self.assertEqual(len(audit.PROBES),12)
        self.assertEqual(audit.TARGET_HEIGHT,.05)
        self.assertEqual(len(audit.SOURCE_HASH),64)

    def test_actual_601_native_samples_and_dt(self):
        for name,rows in self.samples.items():
            with self.subTest(name=name):
                self.assertEqual(len(rows),601)
                self.assertEqual(rows[-1]["time_s"],6.)
                self.assertTrue(all(abs(row["time_s"]-.01*i)<1e-10
                                    for i,row in enumerate(rows)))

    def test_both_native_runs_conserve_water(self):
        for name,rows in self.samples.items():
            with self.subTest(name=name):
                volume=rows[0]["water_volume_m3"]
                self.assertLessEqual(max(abs(row["water_volume_m3"]-volume)
                                         for row in rows),1e-8)
                self.assertAlmostEqual(volume,.6644,places=9)

    def test_both_native_runs_project_wet_divergence(self):
        for name,rows in self.samples.items():
            with self.subTest(name=name):
                self.assertLessEqual(
                    max(row["max_projection_divergence_s_inv"]
                        for row in rows),1e-6)

    def test_all_twelve_real_experimental_channels(self):
        originals,_,meta=audit.official.load()
        self.assertEqual(meta["source_zip_sha256"],audit.SOURCE_HASH)
        for name,rows in self.samples.items():
            for channel in audit.PROBES:
                with self.subTest(grid=name,channel=channel):
                    self.assertEqual(sum(1 for row in rows
                                         if channel in row),601)
                    self.assertTrue(all(
                        isinstance(audit.official.lerp(originals,row["time_s"],channel),float)
                        for row in rows[1::60]))

    def test_twelve_forged_native_logs_rejected_each_grid(self):
        for name,content in self.texts.items():
            with self.subTest(grid=name):
                result=audit.regression_negatives(content,name)
                self.assertEqual(len(result),12)
                self.assertTrue(all(result.values()))

    def test_wrong_grid_rejected(self):
        with self.assertRaises(ValueError):
            audit.parse(self.texts["coarse"],"fine")
        with self.assertRaises(ValueError):
            audit.parse(self.texts["fine"],"coarse")

    def test_truncated_600_step_process_rejected(self):
        for name,content in self.texts.items():
            with self.subTest(grid=name):
                with self.assertRaises(ValueError):
                    audit.parse("\n".join(content.splitlines()[:-1])+"\n",name)

    def test_false_full_phase_marker_rejected(self):
        for name,content in self.texts.items():
            with self.subTest(grid=name):
                fake=content.replace(
                    "phase_complete=false","phase_complete=true")
                with self.assertRaises(ValueError):
                    audit.parse(fake,name)

    def test_full_physics_scope_stays_open(self):
        result=audit.compare()
        self.assertTrue(result["numerical_partial_accepted"])
        self.assertFalse(result["physical_full_p2_validated"])
        self.assertFalse(result["phase_complete"])
        self.assertFalse(result["three_spatial_grids_completed"])
        self.assertFalse(result["dynamic_two_way_FSI_validated"])
        self.assertEqual(len(result["grids"]),2)
        self.assertEqual(len(result["fine_heights_within_0_05m"]),4)
        self.assertEqual(len(result["pressure_rmse_improve_on_refinement"]),8)

if __name__=="__main__":
    unittest.main(verbosity=2)
