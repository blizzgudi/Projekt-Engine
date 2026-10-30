# Experiment voxel_water_001

Testobjekt: Voxelbehaelter 16 x 12 x 8 Zellen, Zellkante 10 cm, Wasser und Wand mit gemeinsam genutzten Materialdefinitionen.

## Start

    cd "D:\Projekt Engine\ENGINE TEST STUBE\experiments\voxel_water_001"
    C:\Python314\python.exe -m unittest -v test_voxel_lab.py
    C:\Python314\python.exe voxel_lab.py
    C:\Python314\python.exe visualize.py

## Nachweise
- output/frame_000.obj, frame_008.obj, frame_030.obj (mit .mtl)
- output/vergleich_querschnitt.png fuer Start-/End-Querschnitt
- output/report.json fuer Zellzahlen und Massenerhaltung
- D:\Projekt Engine\ENGINE TEST STUBE\benchmarks\voxel_water_001.json fuer CPU-Benchmark
- D:\Projekt Engine\ENGINE TEST STUBE\runs\ fuer Testprotokolle

## Wissenschaftliche Grenzen
Keine echte Fluidsimulation, kein Druckmodell, keine freie Oberflaeche, keine echten Viskositaetskraefte. Die Dichte wird nur zur Gesamtmasse verwendet. Keine GPU-Zeitmessung, keine Unreal-Integration. Die Obj-Geometrie zeigt vorhandene Oberflaechen ohne effiziente Chunk-Laufzeitverwaltung.
