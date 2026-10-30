"""Minimaler, engine-unabhaengiger Voxel-Wassertest (keine echte Fluidphysik)."""
from dataclasses import dataclass
from pathlib import Path
import json

EMPTY, WATER, WALL = 0, 1, 2
MATERIALS = {
    WATER: {"name": "Wasser", "density_kg_m3": 1000.0, "viscosity_pa_s": 0.001, "color": (0.12, 0.52, 0.94)},
    WALL: {"name": "Wand", "density_kg_m3": 2400.0, "color": (0.42, 0.45, 0.48)},
}
SIZE = (16, 12, 8)
CELL_SIZE_METERS = 0.10


@dataclass
class VoxelWorld:
    nx: int = SIZE[0]
    ny: int = SIZE[1]
    nz: int = SIZE[2]

    def __post_init__(self):
        self.cells = {}

    def get(self, p):
        return self.cells.get(p, EMPTY)

    def set(self, p, material):
        x, y, z = p
        if not (0 <= x < self.nx and 0 <= y < self.ny and 0 <= z < self.nz):
            raise ValueError("Voxelposition ausserhalb des Rasters")
        if material == EMPTY:
            self.cells.pop(p, None)
        else:
            self.cells[p] = material

    def count(self, material):
        return sum(v == material for v in self.cells.values())

    def initialized(self):
        # Geschlossener Behaelter, oben offen. Anfangs Wasser links.
        for x in range(self.nx):
            for y in range(self.ny):
                for z in range(self.nz):
                    if z == 0 or x in (0, self.nx - 1) or y in (0, self.ny - 1):
                        self.set((x, y, z), WALL)
                    elif x <= 6 and z in (3, 4, 5, 6):
                        self.set((x, y, z), WATER)
        return self

    def step(self, tick):
        """Einfaches diskretes Bewegungsmodell, NICHT Navier-Stokes/FLIP."""
        moved = 0
        touched = set()
        coords = sorted((p for p, t in self.cells.items() if t == WATER), key=lambda p: (p[2], p[1], p[0]))
        dirs = ((1, 0), (0, 1), (-1, 0), (0, -1))
        rotated = dirs[tick % 4:] + dirs[:tick % 4]
        for p in coords:
            if p in touched or self.get(p) != WATER:
                continue
            x, y, z = p
            candidates = [(x, y, z - 1)] + [(x + dx, y + dy, z) for dx, dy in rotated]
            for q in candidates:
                qx, qy, qz = q
                if not (0 <= qx < self.nx and 0 <= qy < self.ny and 0 <= qz < self.nz):
                    continue
                if q not in touched and self.get(q) == EMPTY:
                    self.set(q, WATER)
                    self.set(p, EMPTY)
                    touched.add(q)
                    touched.add(p)
                    moved += 1
                    break
        return moved

    def export_obj(self, path):
        """Erzeugt nur exponierte Flaechen, nicht einen Wuerfel-Actor pro Voxel."""
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.with_suffix(".mtl").write_text(
            "newmtl Wasser\nKd 0.12 0.52 0.94\nd 0.75\n\n"
            "newmtl Wand\nKd 0.42 0.45 0.48\nd 1.0\n", encoding="utf-8"
        )
        faces = [
            ((1, 0, 0), ((1, 0, 0), (1, 1, 0), (1, 1, 1), (1, 0, 1))),
            ((-1, 0, 0), ((0, 0, 0), (0, 0, 1), (0, 1, 1), (0, 1, 0))),
            ((0, 1, 0), ((0, 1, 0), (0, 1, 1), (1, 1, 1), (1, 1, 0))),
            ((0, -1, 0), ((0, 0, 0), (1, 0, 0), (1, 0, 1), (0, 0, 1))),
            ((0, 0, 1), ((0, 0, 1), (1, 0, 1), (1, 1, 1), (0, 1, 1))),
            ((0, 0, -1), ((0, 0, 0), (0, 1, 0), (1, 1, 0), (1, 0, 0))),
        ]
        # Wasser-/Wand-Uebergaenge erhalten eigene Flaechen.
        vertex_count = 0
        face_count = 0
        with path.open("w", encoding="utf-8") as f:
            f.write(f"mtllib {path.stem}.mtl\n")
            for kind in (WALL, WATER):
                f.write(f"g {MATERIALS[kind]['name']}\nusemtl {MATERIALS[kind]['name']}\n")
                for (x, y, z), material in sorted(self.cells.items()):
                    if material != kind:
                        continue
                    for (dx, dy, dz), corners in faces:
                        if self.get((x + dx, y + dy, z + dz)) == kind:
                            continue
                        for ox, oy, oz in corners:
                            f.write(f"v {x + ox} {y + oy} {z + oz}\n")
                        a = vertex_count + 1
                        f.write(f"f {a} {a+1} {a+2}\nf {a} {a+2} {a+3}\n")
                        vertex_count += 4
                        face_count += 2
        return {"triangles": face_count, "vertices": vertex_count}


def main():
    root = Path(__file__).resolve().parent
    output = root / "output"
    world = VoxelWorld().initialized()
    initial_water = world.count(WATER)
    initial_walls = world.count(WALL)
    records = []
    for t in range(31):
        if t in (0, 8, 30):
            path = output / f"frame_{t:03d}.obj"
            mesh = world.export_obj(path)
            records.append({"frame": t, "water_cells": world.count(WATER), "wall_cells": world.count(WALL), **mesh, "file": str(path)})
        if t < 30:
            world.step(t)
            assert world.count(WATER) == initial_water, "Wasservolumen nicht erhalten"
            assert world.count(WALL) == initial_walls, "Wandbeschaedigung"
    volume_m3 = initial_water * CELL_SIZE_METERS ** 3
    data = {
        "note": "Spielzeugmodell diskreter Zellenbewegung; kein physikalisch genauer Fluidsolver",
        "grid": SIZE,
        "cell_size_m": CELL_SIZE_METERS,
        "materials": MATERIALS,
        "initial_water_cells": initial_water,
        "water_volume_m3": volume_m3,
        "water_mass_kg": volume_m3 * MATERIALS[WATER]["density_kg_m3"],
        "frames": records
    }
    output.joinpath("report.json").write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(data, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
