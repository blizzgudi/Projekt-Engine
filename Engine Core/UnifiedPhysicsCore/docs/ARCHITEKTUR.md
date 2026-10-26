# Unified Physics Core – Architektur v0.2

Status: Native CPU-C++17-Kernbibliothek. Physiktests erfolgreich.
P2 ist als geprüfter 3D-Wassersolver-MVP abgegrenzt, nicht als vollständiger
3D-CFD- oder AAA-Liquid-Solver. UE5.8-Adapter wurde früher kompiliert und
initialisiert, rendert aber die neue 3D-Domäne noch nicht (P8 offen).

## Schichten

1. MaterialLibrary: gemeinsame unveränderliche Material-ID, Phase und Dichte
   sowie weitere Parameter als Metadaten. Viskosität steuert v0.2 noch nicht.
2. VoxelGrid: dichtes 3D-Materialraster, Füllanteil, Temperatur und Schaden.
   Noch nicht mit 3D-Wasser gekoppelt – das ist Phase P3.
3. ShallowWater2D: älterer 2D-Druck- und Höhensolver.
4. MacProjection3D: freie-Oberflächen-Drucklösung auf dreidimensionalem
   gestaffeltem Geschwindigkeitsgitter (Zellen: Druck, Faces: Geschwindigkeiten).
5. CoupledMacWater3D: nutzt denselben MAC-Projektor und [0,1]-Wasseranteile
   im dreidimensionalen Gitter. Pro Teilschritt: Occupancy aktualisieren,
   Geschwindigkeit advektieren, Schwerkraft, Druckprojektion,
   Wasseranteile durch konservative Flüsse transportieren.
6. UnifiedPhysicsRuntime: bindet 3D-Flüssigkeiten an gemeinsame Material-IDs
   und Dichten und besitzt zentralen Update-Aufruf.
7. Experimentelle zukünftige Materialsysteme (in materials_ext.hpp,
   solid.hpp): getrennte Prototypen; keine behauptete Mehrphysik-Kopplung.

## Numerische Nachweise

- CMake/CTest mit MSVC Release (9 Testgruppen bei P2-Abnahme).
- Flüssigkeitsvolumen und stabile Wasseranteile bei 3D-Dammbruch.
- Vergleich des Bodendrucks im ruhenden 3D-Becken mit rho*g*h.
- Durchgehende innere 3D-Hindernisse, keine Leckage.
- Vergleiche über Gittergrößen und gemessene CPU-Runtimes; rechnerische
  Datenfeld-Speicherbudgetwerte (keine Prozess-Peak-Messungen).
- Reproduzierbare Acceptance mit SHA256 aller kritischen P2-Quelldateien.

## Grenzen

Das Materialraster ist weiterhin nicht identisch mit der 3D-Wasseroccupancy,
und das Gesamtsystem hat noch keine konservative Kopplung von Voxelmaterialien,
Wärme, Impuls und chemischer Reaktion. Die wasserführenden Zellen besitzen
keinen Level-Set oder geometrischen VOF-Interface-Rekonstruktor.
Der Druckprojektor skaliert ohne Multigrid schlecht. Es gibt weder GPU
noch FSI oder Unreal-3D-Runtime-Nachweis. Keiner dieser offenen Punkte
wird durch ein PASS der begrenzten Phase P2 implizit geschlossen.
