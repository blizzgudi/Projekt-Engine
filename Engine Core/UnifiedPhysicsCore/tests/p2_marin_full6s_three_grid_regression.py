r"""P2 PARTIAL: real SPHERIC Test-02 6s three-grid experimental QA.
No full phase PASS. Includes corrupted-native-evidence rejection."""
import sys
import unittest
from pathlib import Path
CORE=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(CORE/"scripts"))
import p2_marin_full6s_three_grid_audit as audit
import p2_spheric_marin_compare as official

class NativeThreeGridExperimentalSixSeconds(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data={}
        cls.raw={}
        for grid,file in audit.FILES.items():
            cls.raw[grid]=file.read_text(encoding="utf-8")
            cls.data[grid]=audit.parse(cls.raw[grid],grid)

    def test_native_independent_3d_shape_and_spacing(self):
        self.assertEqual(audit.SHAPES,{"coarse":(32,10,10),
                                        "medium":(48,15,15),
                                        "fine":(64,20,20)})
        self.assertEqual(audit.GRIDS,("coarse","medium","fine"))
        for grid in audit.GRIDS:
            nx,ny,nz=audit.SHAPES[grid]
            self.assertAlmostEqual(nx*audit.DX[grid],3.2)
            self.assertAlmostEqual(ny*audit.DX[grid],1.)
            self.assertAlmostEqual(nz*audit.DX[grid],1.)

    def test_full_601_independent_realtime_samples_per_grid(self):
        for grid,rows in self.data.items():
            with self.subTest(grid=grid):
                self.assertEqual(len(rows),601)
                self.assertEqual(rows[0]["time_s"],0)
                self.assertEqual(rows[-1]["time_s"],6)
                self.assertTrue(all(abs(row["time_s"]-i*.01)<1e-10
                                    for i,row in enumerate(rows)))

    def test_native_real_fluid_incompressibility_every_grid(self):
        for grid,rows in self.data.items():
            with self.subTest(grid=grid):
                self.assertLessEqual(max(row["max_projection_divergence_s_inv"]
                                         for row in rows),1e-6)

    def test_native_real_fluid_volume_conservation_every_grid(self):
        for grid,rows in self.data.items():
            with self.subTest(grid=grid):
                volume=rows[0]["water_volume_m3"]
                self.assertAlmostEqual(volume,.6644,places=8)
                self.assertLessEqual(
                    max(abs(row["water_volume_m3"]-volume) for row in rows),1e-8)

    def test_real_unmodified_12_channel_measurement_metadata(self):
        measured,_,meta=official.load()
        self.assertEqual(meta["source_zip_sha256"],audit.EXPECTED_SHA)
        self.assertEqual(len(measured),7395)
        for channel in audit.HEIGHTS+audit.PRESSURES:
            self.assertIn(channel,measured[0])
            self.assertTrue(all(channel in row for grid in audit.GRIDS
                                for row in self.data[grid][1::60]))

    def test_all_36_independent_negative_protocol_mutations(self):
        for grid,raw in self.raw.items():
            with self.subTest(grid=grid):
                results=audit.native.regression_negatives(raw,grid)
                self.assertEqual(len(results),12)
                self.assertTrue(all(results.values()))

    def test_forged_complete_p2_marker_never_accepted(self):
        for grid,raw in self.raw.items():
            with self.subTest(grid=grid):
                bad=raw.replace("phase_complete=false","phase_complete=true")
                with self.assertRaises(ValueError):
                    audit.parse(bad,grid)

    def test_grid_switch_forbidden(self):
        for left,right in (("coarse","medium"),("medium","fine"),
                           ("fine","coarse")):
            with self.subTest(from_grid=left,as_grid=right):
                with self.assertRaises(ValueError):
                    audit.parse(self.raw[left],right)

    def test_single_missing_experimental_timestep_rejected(self):
        for grid,raw in self.raw.items():
            rows=raw.splitlines()
            damaged="\n".join(rows[:100]+rows[101:])+"\n"
            with self.subTest(grid=grid):
                with self.assertRaises(ValueError):
                    audit.parse(damaged,grid)

    def test_physical_cfd_not_approved_by_numerical_integrity(self):
        report=audit.compare()
        self.assertTrue(report["numerical_partial_accepted"])
        self.assertEqual(report["independent_native_spatial_grid_count"],3)
        self.assertFalse(report["complete_phase"])
        self.assertFalse(report["experimental_three_grid_cfd_validated"])
        self.assertFalse(report["bidirectional_dynamic_3d_FSI_validated"])
        self.assertEqual(len(report["fine_height_acceptance"]),4)
        self.assertEqual(len(report["fine_pressure_rmse_pa"]),8)
        self.assertEqual(len(report["rmse_refinement_monotonic"]),12)
        self.assertGreater(len(report["unmet"]),0)

if __name__=="__main__":
    unittest.main(verbosity=2)
