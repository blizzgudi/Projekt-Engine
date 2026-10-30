import json
import unittest
import hashlib
from pathlib import Path
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parent
OUT=ROOT/"output"
with (OUT/"report.json").open(encoding="utf-8") as fh:
    META=json.load(fh)
DATA=np.fromfile(OUT/"voxel_material_ids.bin",dtype=np.uint8).reshape((META["nz"],META["ny"],META["nx"]))
RENDER=json.loads((OUT/"render_validation.json").read_text(encoding="utf-8"))

class TreeTrunkProof(unittest.TestCase):
    def test_real_upc_material_id_layers(self):
        self.assertEqual([m["upc_material_id"] for m in META["materials"]],[1101,1102,1103,1104])
        self.assertEqual(META["source"],"real UPC MaterialLibrary and VoxelGrid")
        for m in META["materials"]:
            self.assertEqual(int(np.count_nonzero(DATA==m["voxel_id"])),m["voxel_count"])
        self.assertEqual(int(np.count_nonzero(DATA)),META["occupied_voxels"])

    def test_ordered_cross_section_and_taper(self):
        z=int(META["nz"]*.74)
        mid=META["nx"]//2
        cross=DATA[z,mid,mid:]
        nonzero=cross[cross!=0]
        # Interior-to-exterior materials must be arranged radially.
        first=[int(nonzero[0])]
        for m in nonzero[1:]:
            if int(m)!=first[-1]:first.append(int(m))
        self.assertEqual(first,[1,2,3,4])
        lower=int(np.count_nonzero(DATA[1]))
        upper=int(np.count_nonzero(DATA[-2]))
        self.assertGreater(lower,upper)

    def test_mass_computed_from_upc(self):
        self.assertAlmostEqual(META["mass_kg_upc"],META["mass_kg_independent"],places=5)
        self.assertGreater(META["mass_kg_upc"],0)

    def test_mesh_contains_actual_faces(self):
        self.assertGreater(RENDER["full_obj_quads"],1000)
        self.assertGreater(RENDER["cutaway_obj_quads"],1000)
        for n in ("tree_trunk_full","tree_trunk_cutaway"):
            mesh=(OUT/(n+".obj")).read_text(encoding="ascii")
            self.assertIn("usemtl Rinde",mesh)
            self.assertIn("usemtl Splintholz",mesh)
            self.assertIn("usemtl Kernholz",mesh)
            self.assertTrue((OUT/(n+".mtl")).is_file())

    def test_exact_binary_used_and_preview_files(self):
        src=(OUT/"voxel_material_ids.bin").read_bytes()
        self.assertEqual(hashlib.sha256(src).hexdigest(),RENDER["binary_sha256"])
        for f in ["tree_trunk_full.png","tree_trunk_cutaway.png","tree_trunk_cross_section.png",
                  "tree_trunk_print_progress.png","tree_trunk_overview.png"]:
            with Image.open(OUT/f) as im:
                self.assertGreaterEqual(im.width,250)
                im.verify()

if __name__=="__main__":
    unittest.main(verbosity=2)
