# Unified Physics Core – Architektur P0–P5 v0.5

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

10. SpringSolid3D/SpringLattice3D (P5): 3D-Materialknoten mit
    26fachen räumlichen Federbindungen, expliziten Kräften,
    Stabilitätssubsteps, Schwellenbruch und Fragment-Union-Find.
    Bruch-Bondanteile werden über die ursprünglichen Material-IDs
    als Damage in das zentrale VoxelGrid übertragen.
11. SolidPlaneContact3D (P5): Knoten-/Wandreibung und
    Restitutionsimpulse gegen eine feste Ebene.
12. GranularWorld3D (P5): Material-ID-/Dichte-gesteuerte
    sphärische Partikel mit sechs Wandebenen und gegenseitigen
    Normalkollisions-/Coulomb-Tangentialimpulsen,
    12 Iterationen und adaptiver Kornweg-Schrittweite.
    Granular-Density, Bruch und Kontakt sind eigenständige
    numerische Domänen; kein erzwungener gemeinsamer
    Solverkern oder vollständig konservative FSI.

13. DeformableContact3D (P5): Ungebundene massbasierte
    Volumensphären an einem/mehreren SpringSolid3D-Objekten.
    Paarweise Selbst- und Mehrkörper-Nichtdurchdringung durch
    wiederholte Impuls-/Reibkontakte und substep-limitierten
    Kontaktlöseprozess. Nach Federbruch dürfen die ehemaligen
    Nachbarn wieder gegenseitig stoßen. Kollisions-Energieverluste
    und freigesetzte elastische Bruchenergie getrennt ausgewiesen.
    Kein kontinuierliches verformbares Triangle-Mesh-Kontaktmodell.
14. P5-Konstitutiv-Validierung: affine 3D-Zug-/Scherlast
    an Gittergrößen 4³,6³,8³. Relative Kraft/Dehnung und
    Energie/Dehnung auf numerische kleine-Dehnungs-
    Linearisierung geprüft, nicht gegen reale Labormesswerte.

## Verifikation
- Release-MSVC-CMake, CTest, P1/P2/P3/P4/P5-Abnahmeskripte.
- Unabhängige numerische Referenzen: hydrostatischer Druck,
  Ritter/Stoker 2D-Dammbruch, 3D MAC/VOF, Darcy-Vertikalfluss,
  Neumann-Fourier-Eigenmode (4 Raster).
- Energie-/Massenprüfungen im jeweils abgegrenzten Subsystem,
  Phase-Rücktransfers zu MAC und getrennte Porenbilanzen.
- Quell-Fingerprints nach jedem P1–P5-Abnahmelauf.

## Grenzen, nicht als gelöst ausgeben
Globale Energie von MAC-FSI/Turbulenz, 3D-Gas-/Dampfdruck- und
Expansionsdynamik, kapillar-thermische Pore/Skelett-Kontaktmodelle,
mehrphasige Mischungsströmungen, kalibrierte Kontinuumsfestkörper,
rollende Drehmomente und GPU-beschleunigte Partikelkontakte,
GPU-/LOD-Optimierung und Unreal-Rendering/E2E verbleiben als
Forschung bzw. P6–P8. Ein begrenzter P5-CPU-MVP ist kein AAA-Release.
