# Unified Physics Core — v0.2 (CPU-Forschungssystem)

Unreal-unabhängige C++17-Bibliothek für Materialzustände, Voxel und erste
physikalische Simulationen. Entwicklung liegt unter D:\Projekt Engine.

## Verfügbare Module

- MaterialLibrary: zentrale Material-IDs, Phasen, Dichte und Viskositätsdaten.
- VoxelGrid: material_id, Füllanteil, Temperatur und Schaden pro 3D-Zelle.
- ShallowWater2D: konservativer 2D-Wassersolver mit Wasserhöhe,
  Geschwindigkeit und hydrostatischem Bodendruck. Phase P1 ist als
  CPU-2D-MVP geprüft: HLL-Riemannsolver, Trocken-/Nassbett-Ritter/Stoker,
  Konvergenz, 2D-Symmetrie, Hindernisse und Massenerhaltung.
  Abnahme und verbleibende Grenzen: docs/P1_2D_FLUID_ABNAHME.md.
- MacProjection3D: versetztes Geschwindigkeitsgitter, Druckprojektion,
  freie Oberfläche und feste Grenzen.
- CoupledMacWater3D: **zusammenhängender CPU-3D-Wassersolver** mit MAC-Druck,
  Schwerkraft, semi-Lagrangescher Geschwindigkeitsadvektion, konservativem
  dreidimensionalem Volumentransport und Kollisionen mit festen Hindernissen.
- UnifiedPhysicsRuntime: Eintrittsstelle für gemeinsame Materialbibliothek,
  2D- und 3D-Flüssigkeitsdomänen und zentralen Simulationsschritt.
- thermal3d.hpp: **P4-Teilmeilenstein, noch keine vollständige P4-Abnahme.**
  3D-Wärmeleitung zwischen Festkörper/Wasser mit Energieerhaltung,
  Eis/Wasser/Dampf-Enthalpie, Wärmequellen und Material-ID-Snapshot aus
  MAC-Wasser und VoxelGrid; thermische Festkörpertemperaturen zurück in
  VoxelGrid. Keine laufende thermische Fluidadvektion, keine echte
  Eis-/Dampfvolumen-Rückkopplung. Details: docs/P4_THERMAL_PARTIAL_ACCEPTANCE.md.
- material_coupling3d.hpp: **Phase P3 – 3D Material-/Volumen-Kopplung als CPU-MVP:**
  Boden-IDs mit Porosität/Durchlässigkeit, MAC-Wasserinfiltration und
  Porenversickerung, Massenerhaltung, Energieverlust- und Druckkraftnachweise
  sowie ein voxelquantisierter beweglicher Starrkörper mit konservativer
  Wasserverdrängung. docs/P3_MATERIAL_VOLUME_ABNAHME.md erläutert Grenzen:
  kein validiertes vollumfängliches dynamisches Fluid-Festkörper-FSI.
- materials_ext.hpp: Prototypen für Materialmischung, Wasserenthalpie/
  Eis/Wasserdampf, Wasser-Boden-Infiltration und sparse 16³-Voxel-Chunks.
- solid.hpp: einfacher 3D-Masse-Feder-Prototyp mit Bruchgrenze.

## Phase P2

P2 ist als CPU-3D-Prototyp und Verfahrensentscheidung abgeschlossen, wenn
scripts/p2_acceptance.py nach MSVC-Release-Build PASS meldet. Siehe
docs/P2_3D_FLUID_ABNAHME.md für Methodik, Messdaten und Grenzen.

Die anderen Phasen bleiben eigenständig offen: keine Mehrmaterial-Fluid-
Festkörperkopplung, keine Wärmeausbreitung im 3D-Gitter, keine kalibrierte
Reaktionsphysik, kein performanter GPU-Solver und kein UE5-Visual-E2E.

## Reproduzierbarer Test

CMake mit MSVC C++17 konfigurieren, Release bauen und CTest ausführen.
Anschließend: C:\Python314\python.exe scripts\p1_acceptance.py
Dann: C:\Python314\python.exe scripts\p2_acceptance.py
Zuletzt: C:\Python314\python.exe scripts\phase_gate.py

Quantitative P2-Belege liegen ausschließlich unter
D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_acceptance.json.
Das Benchmark-Ergebnis ist nicht als realtime/AAA-Spielqualität zu verstehen.

## Architekturprinzip

Eine gemeinsame Materialdatenbasis, ein zentraler Runtime-Einstiegspunkt und
austauschbare numerische Solver. Unreal ist ein optionaler Adapter und darf
die Kernphysik nicht definieren. Nicht implementierte Stoffparameter dürfen
nicht als physikalisch aktive Regeln ausgegeben werden.
