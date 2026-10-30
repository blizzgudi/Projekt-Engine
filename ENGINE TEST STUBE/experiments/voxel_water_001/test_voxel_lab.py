import unittest
from voxel_lab import VoxelWorld, WATER, WALL, EMPTY, SIZE

class VoxelTest(unittest.TestCase):
    def test_material_references(self):
        world = VoxelWorld().initialized()
        self.assertGreater(world.count(WATER), 0)
        self.assertGreater(world.count(WALL), 0)
        self.assertEqual(world.get((1, 1, 7)), EMPTY)

    def test_water_quantity_and_solid_cells(self):
        world = VoxelWorld().initialized()
        water = world.count(WATER)
        solids = {p for p, kind in world.cells.items() if kind == WALL}
        for t in range(30):
            world.step(t)
            self.assertEqual(world.count(WATER), water)
            self.assertTrue(all(world.get(p) == WALL for p in solids))

    def test_out_of_bounds(self):
        world = VoxelWorld()
        with self.assertRaises(ValueError):
            world.set((-1, 2, 3), WATER)

if __name__ == "__main__":
    unittest.main()
