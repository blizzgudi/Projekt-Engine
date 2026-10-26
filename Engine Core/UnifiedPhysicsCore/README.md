# Unified Physics Core v0.7 – Standalone C++17 CPU-Enginekern

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

- P5: **vollständig getesteter abgegrenzter CPU-MVP** mit volumetrischem
  3D-Federgitter, Material-IDs, Hooke-/strain-basiertem Bruch, Fragmenten,
  VoxelGrid-Schadensexport; 3D-Kugelgranulat (Korn-Korn/Wand,
  Rückprall/Reibung/Stapel), nichtgebundenem deformierbarem
  Selbst-/Mehrkörperkontakt und affine Zug-/Scherkraft-Benchmarks.
  Grenzen: docs/P5_SOLID_GRANULAR_CPU_MVP_ABNAHME.md.

- P6: Mehrspezies-Voxelgemische mit registrierten Elementmassen,
  ausgeglichener Reaktions-Stöchiometrie, Arrhenius-1.-Ordnung-Regeln,
  konservativer 3D-Fick-Diffusion, Reaktionsenthalpie-Bilanz,
  dominantem VoxelGrid-Material-Proxy und Wärmeabgabe an eine
  benachbarte P4-Festkörperzelle. Synthetic-CPU-MVP, **keine**
  automatischen echten Chemievorhersagen.

- P8: Unreal Engine 5.8 Native-Adapter mit echter 3D-MAC/VOF-
  Wassersimulation, dynamischem Unreal-InstancedStaticMesh-Rendering,
  native Game- und Editor-PIE-Sitzungen mit ExitCode=0, zwei echten
  GPU-SceneCapture-PNGs 960x540 und reproduzierbarem Quellen-/
  Binär-/Rendering-E2E-Nachweis. Nur P2-Wasser visuell angebunden,
  P3–P6 UE-Gesamtszenen bleiben zukünftiger Ausbau.
  Nachweis: docs/P8_UNREAL58_NATIVE_E2E_ABNAHME.md.

- P7: **Hardware-GPU/Streaming-Prototyp abgenommen**: Direct3D11-
  Compute-Shader macht reale 3D-MAC/VOF-Render-LOD-Aggregation, unabhängige
  CPU-Sparse-Vergleiche auf 5 Volumengrößen und vier LOD-Stufen.
  Vulkan/CUDA/WARP nicht benötigt. Konservatives 8³-Kachel-Streaming
  mit Frame-Budget und echtem GPU-VRAM-Upload plus Readback-Beweis.
  Die GPU beschleunigt *nicht den eigentlichen Physiksolver* und
  kein Unreal-RDG-/Framezeitgewinn ist versprochen.
  Siehe docs/P7_GPU_LOD_STREAMING_CPU_GPU_ABNAHME.md.

## Noch nicht abgeschlossen
*Keine offenen Phasen im abgegrenzten P0–P8-Prototyp-Plan.* Eine produktive
AAA-Engine mit GPU-Festkörper-/Fluidsolver, Unreal-RDG, großflächiger
World-Streaming-Integration und kompletten Visualisierung P3–P6 ist ein
separater zukünftiger Ausbau.


## Referenznachweise
docs/P1_2D_FLUID_ABNAHME.md
docs/P2_3D_FLUID_ABNAHME.md
docs/P3_MATERIAL_VOLUME_ABNAHME.md
docs/P4_THERMAL_CPU_MVP_ABNAHME.md
docs/P5_SOLID_GRANULAR_CPU_MVP_ABNAHME.md
docs/P6_REACTIVE_MATERIALS_CPU_MVP_ABNAHME.md
docs/P7_GPU_LOD_STREAMING_CPU_GPU_ABNAHME.md
docs/P8_UNREAL58_NATIVE_E2E_ABNAHME.md
docs/PHASENPLAN.md

Wichtige Einschränkung: P4, P5 und P6 sind abgegrenzte CPU-MVPs. Sie sind keine
universellen Thermo-FSI-/FEM-/MPM-/DEM-Systeme. P4 ist kein
vollständiger 3D-Steam/Gas/thermoelastischer Solver. Das Dampfvolumen
wird diagnostiziert, aber nicht in einem Gas-Druckfeld bewegt.
Der thermische Energiehaushalt ist für definierte Subsysteme erhalten;
keine globale kinetische Energie-Rückkopplung aller Physics-Module.

## Build und Qualitätsprüfung
- CMake MSVC-Release bauen und ctest --output-on-failure ausführen.
- Dann alle scripts/p1_acceptance.py,
  scripts/p2_acceptance.py, scripts/p3_acceptance.py,
  scripts/p4_partial_acceptance.py, scripts/p4_acceptance.py,
  scripts/p5_contact_partial.py, scripts/p5_acceptance.py,
  scripts/p5_full_acceptance.py, scripts/p6_acceptance.py,
  scripts/p7_sparse_partial_acceptance.py, scripts/p7_acceptance.py ausführen.
- P8 real Unreal Game+Editor PIE mit ENGINE TEST STUBE/scripts/p8_native_e2e.py
  game und pie starten, danach scripts/p8_acceptance.py ausführen.
- Abschließend scripts/phase_gate.py.
- Maschinenlesbare Nachweise liegen unter:
  D:\Projekt Engine\ENGINE TEST STUBE\runs\

Git bleibt eigenständig lokal unter D:\Projekt Engine; kein Push zum
fremden AI-Game-Studio-Repository.
