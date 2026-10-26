# P7 Teilabnahme — konservative Sparse-Brick-LOD-Snapshots der echten 3D-MAC/VOF-Quelle

Stand 2026-10-26. **P7 bleibt PARTIAL**. Dieser Nachweis deckt ausschließlich
den CPU-seitigen, konservativen 3D-Wasser-LOD-Export ab, nicht GPU-Compute,
GPU/CPU-Gleichwertigkeit, LOD-basierte Simulation, Streaming-Latenz oder
Unreal-Rendering. Er ist ausdrücklich **keine vollständige P7-Abnahme**.

## Implementierung

- \`include/upc/sparse_water_lod3d.hpp\`: \`SparseWaterLod3D\` liest
  direkt \`CoupledMacWater3D\` (echter P2-MAC/VOF-Zustand), ohne ihn zu
  verändern. Alle LOD-Stufen sind 3D und verwenden Sparse-Bricks zu
  8×8×8 Samples. Nur Bricks mit Wasser oder Feststoff werden allokiert.
  Fehlende Bricks werden als leer interpretiert.
- Wasserfüllgrad und Feststoffbelegung sind separate Kanäle. Jede
  grobe Zelle enthält die Summe der feinen Füllgrade geteilt durch
  die **tatsächliche** Anzahl abgedeckter feiner Zellen. Dies
  berücksichtigt abgeschnittene Randzellen bei ungeraden Dimensionen.
  \`sum(LOD_fraction * covered_fine_cells * dx³)\` ergibt die
  Quellwassermenge. Die Feststoffbelegung wird analog aggregiert.
- \`rebuild\` ersetzt den vollständigen Snapshot atomar nach
  erfolgreicher Konstruktion. Quelle und Solver bleiben unangetastet.
  Die Bricks sind für eine deterministische Zuordnung in \`std::map\`
  organisiert; der Algorithmus ist CPU-only.
- \`resident_bytes_model()\` zählt ausschließlich Brick-Nutzdaten;
  es ist eine **Untergrenze**, keine vollständige Prozess-/Heapmessung.
  \`dense_bytes_model()\` zählt die entsprechenden dichten
  Sample-Nutzdaten aller Stufen. Ein Sparse-LOD-Snapshot kann für
  dichte Szenen größer sein als die dichte Repräsentation.

## Native Abnahmeszenarien

- \`tests/sparse_water_lod3d_tests.cpp\`: 17×13×9 mit nichtpotenz-
  zweier Randzellen, Feststoffmaske, 6 LOD-Stufen, Einzelzell-
  Füllgrade und konservative Volumenbilanz; Neuaufbau nach
  Zustandsänderung; dünn besetztes 64³-Gitter und Speichervorteil;
  30 reale \`CoupledMacWater3D::step\`-Aufrufe mit 5 LOD-Stufen
  und Massenvergleich nach **jedem** Schritt.
- \`benchmarks/p7_sparse_reference.cpp\`: 32³/48³/64³,
  Quell- und feinste/gröbste LOD-Volumina, aktive Bricks,
  gemessene CPU-Snapshot-Erstellungsdauer und Nutzdatenvergleich.
  Der Benchmark ist nicht mit GPU-Leistung vergleichbar.
- \`scripts/p7_sparse_partial_acceptance.py\`: führt beide nativen
  Executables erneut aus, prüft die numerischen Grenzen und
  SHA-256-Fingerprints und schreibt einen maschinenlesbaren Bericht
  nach \`ENGINE TEST STUBE/runs/p7_sparse_partial.json\`.

## Offene vollständige P7-Abnahme

GPU-Compute-Kernel mit echter Hardware-Ausführung, CPU/GPU-
Referenzvergleich (Masse, Fehlernormen, Laufzeit), definierte
GPU-Speicher- und Transferbudgets, Streaming/LOD-Umschaltung
mit Latenzgrenzen sowie belastbare Skalierung auf größeren
aktiven Welten. Erst dann darf P7 auf PASS wechseln.
