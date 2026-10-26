# Unified Physics Core v0.4 – Standalone C++17 CPU-Enginekern

Projekt unter D:\Projekt Engine. Unreal 5.8 ist ein optionaler Adapter.
Das Originalsystem wird modular in Phasen P0 bis P8 entwickelt und lokal
mit MSVC Release + CTest nachgewiesen.

## Nachgewiesene CPU-Prototypen
- P0: MaterialLibrary, VoxelGrid, reproduzierbarer Build.
- P1: Konservativer 2D-Wassersolver, Ritter/Stoker, Druck und Hindernisse.
- P2: Gekoppelter 3D-MAC+VOF-Flüssigkeitssolver, Wasser/Schwerkraft,
  Druck, Volumenerhaltung, feste Hindernisse, 3D-Dammbruch.
- P3: Material-ID-basierte 3D-Poren/Boden-Kopplung, feste und
  quantisiert bewegliche Voxel, lokaler Wasser-/Energiehaushalt.
- P4: ThermalVolume3D, konservativer 3D-Fourier-Wärmefluss,
  Eis/Wasser/Dampf-Enthalpie, vollfrierende feste MAC-Zellen,
  liquide/Steam-Volumendiagnose, live konservativer MAC-Massen-
  und Enthalpietransport, P3-Darcy-Gravitationsenergie-zu-Wärme.

## Noch nicht abgeschlossen
P5: Festkörper/Bruch/granulare Kontakte.
P6: Dynamische Materialmischungen und Reaktionsphysik.
P7: GPU/LOD und Skalierung.
P8: Unreal5.8-Gameplay-/Rendering- und End-to-End-Test.

## Referenznachweise
docs/P1_2D_FLUID_ABNAHME.md
docs/P2_3D_FLUID_ABNAHME.md
docs/P3_MATERIAL_VOLUME_ABNAHME.md
docs/P4_THERMAL_CPU_MVP_ABNAHME.md
docs/PHASENPLAN.md

Wichtige Einschränkung: P4 ist ein vereinfachter CPU-MVP, kein
vollständiger 3D-Steam/Gas/thermoelastischer Solver. Das Dampfvolumen
wird diagnostiziert, aber nicht in einem Gas-Druckfeld bewegt.
Der thermische Energiehaushalt ist für definierte Subsysteme erhalten;
keine globale kinetische Energie-Rückkopplung aller Physics-Module.

## Build und Qualitätsprüfung
- CMake MSVC-Release bauen und ctest --output-on-failure ausführen.
- Dann alle scripts/p1_acceptance.py,
  scripts/p2_acceptance.py, scripts/p3_acceptance.py,
  scripts/p4_partial_acceptance.py, scripts/p4_acceptance.py ausführen.
- Abschließend scripts/phase_gate.py.
- Maschinenlesbare Nachweise liegen unter:
  D:\Projekt Engine\ENGINE TEST STUBE\runs\

Git bleibt eigenständig lokal unter D:\Projekt Engine; kein Push zum
fremden AI-Game-Studio-Repository.
