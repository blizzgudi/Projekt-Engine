"""CPU-only baseline; not a graphics or fluid-accuracy benchmark."""
from pathlib import Path
from time import perf_counter
import json
import platform
import tracemalloc

from voxel_lab import VoxelWorld, WATER, WALL, CELL_SIZE_METERS

def run_case(nx, ny, nz, steps=30):
    tracemalloc.start()
    start = perf_counter()
    world = VoxelWorld(nx, ny, nz).initialized()
    init_sec = perf_counter() - start
    water = world.count(WATER)
    walls = world.count(WALL)
    step_start = perf_counter()
    movements = 0
    for tick in range(steps):
        movements += world.step(tick)
    step_sec = perf_counter() - step_start
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    assert world.count(WATER) == water
    assert world.count(WALL) == walls
    return {"dimensions": [nx, ny, nz], "cells_total": nx*ny*nz,
            "water_cells": water, "steps": steps, "movements": movements,
            "init_seconds": round(init_sec, 6),
            "simulation_seconds": round(step_sec, 6),
            "average_step_ms": round(step_sec/steps * 1000, 3),
            "peak_tracemalloc_bytes": peak,
            "notes": "Python tracemalloc tracks Python allocations, not total process RSS, GPU memory or rendering."}

def main():
    baseline = {
        "description": "CPU benchmark des vereinfachten, diskreten Voxelmodells",
        "platform": platform.platform(), "python": platform.python_version(),
        "cell_size_m": CELL_SIZE_METERS,
        "cases": [run_case(16,12,8), run_case(32,24,16)]
    }
    destination = Path(__file__).resolve().parents[2] / "benchmarks" / "voxel_water_001.json"
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(baseline, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(baseline, indent=2, ensure_ascii=False))

if __name__ == "__main__":
    main()
