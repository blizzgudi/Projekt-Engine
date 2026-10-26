# Unified Physics Core – Architektur P0–P4 v0.4

Eigenständiger C++17-CPU-Physikkern, Unreal Engine nur als
Render-/Test-Adapter. Alle Zustände verwenden SI-Einheiten.

## Komponenten
1. MaterialLibrary und VoxelGrid: Material-IDs, Phasen, Dichte,
   Materialmasse, 3D-Materialzustand und voxelweise Temperatur.
2. ShallowWater2D: Konservative Tiefe/Impuls-Verteilung
   mit HLL-Riemannlösung und reflektierenden Wänden.
3. CoupledMacWater3D + MacProjection3D: MAC-Faces für Geschwindigkeiten,
   Druckprojektion, Schwerkraft und konservativer begrenzter
   Volumentransport. Optionaler Flux-Listener zur
   Energie-/Materialkopplung. Andere P2-Testergebnisse bleiben gültig.
4. MaterialVolumeCoupler3D: P3-Overlay über denselben Material-IDs
   und Fluid-Voxelkoordinaten, vertikale poröse Infiltration sowie
   MAC-Wasserdruck auf starre Voxel und Zellbewegungs-Kollision.
   Optionaler Pore-Transfer-Listener mit Quell-/Zielvolumen und
   isolierter Änderung des Gravitationspotentials.
5. ThermalVolume3D: material-ID-spezifische 3D-Fourier-Wärmeleitung,
   Enthalpie, feste und liquide Zellzustände,
   Phasenverteilung Eis/Wasser/Dampf bei ungefährem Atmosphärendruck.
   Große numerische VOF-Zeitschrittsteifigkeit begrenzt die
   paarweise Wärmestrom-Relaxation, ohne Energie zu erzeugen.
6. ThermalPhaseMacBridge3D: Wasserphasenumwandlung als
   MAC-Wasseranteil, Ice-Blockierung, Vapor-Inventar,
   Temperatur-Writeback zu festem Materialgrid.
7. ThermalMacAdvection3D: Energie- und Massenadvektion auf den
   tatsächlich verwendeten MAC-Volumenflüssen; Teilphasenadvektion
   ohne geeignetes Mehrphasenmodell explizit gesperrt.
8. ThermalPoreExchange3D: Poröse 3D-Materialschichten bekommen
   Enthalpie aus Quellwasser und frei werdende Gravitation
   als Wärme. Die lokale Transferbilanz ist konservativ.
9. thermal_volume_estimates.hpp: Flüssig-/Eisvolumen und
   Wasserdampf als idealgasbasierte Volumendiagnostik.

## Verifikation
- Release-MSVC-CMake, CTest, P1/P2/P3/P4 Acceptance-Skripte.
- Unabhängige numerische Referenzen: hydrostatischer Druck,
  Ritter/Stoker 2D-Dammbruch, 3D MAC/VOF, Darcy-Vertikalfluss,
  Neumann-Fourier-Eigenmode (4 Raster).
- Energie-/Massenprüfungen im jeweils abgegrenzten Subsystem,
  Phase-Rücktransfers zu MAC und getrennte Porenbilanzen.
- Quell-Fingerprints nach jedem P4-Abnahmelauf.

## Grenzen, nicht als gelöst ausgeben
Globale Energie von MAC-FSI/Turbulenz, 3D-Gas-/Dampfdruck- und
Expansionsdynamik, kapillar-thermische Pore/Skelett-Kontaktmodelle,
mehrphasige Mischungsströmungen, Kontinuumsfestkörper,
GPU-/LOD-Optimierung und Unreal-Rendering/E2E verbleiben als
Forschung bzw. P5–P8. Ein CPU-MVP ist kein AAA-Release.
