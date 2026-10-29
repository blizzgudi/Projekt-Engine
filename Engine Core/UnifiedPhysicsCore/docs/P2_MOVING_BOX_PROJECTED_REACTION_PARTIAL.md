# P2 Teilabnahme: projizierter dynamischer MAC-Gegenimpuls (29.10.2026)

**Status: PARTIAL, nicht P2-PASS.** Optionaler Testpfad
`P2MovingBoxProjectedReaction3D` unter
`include/upc/p2_moving_box_projected_reaction3d.hpp`.

## Umfang

1. Berechne Drucktraktion eines vollständig gefluteten achsenparallelen
   Quaders aus vorgegebenem Zell-Druckfeld.
2. Bewege den starren Körper translatorisch und deponiere den Gegenimpuls
   auf benachbarte bewegte MAC-Dualmassen.
3. Führe anschließend eine **gemeinsame primal-MAC-Cut-Cell-PCG-Projektion**
   auf die neue Boxgeometrie aus, inklusive geometrischer Kontinuität.
4. Rekonstruiere MAC-Face-Geschwindigkeiten aus korrigierten offenen
   Volumenflüssen. Erfasse die durch Druckprojektion zusätzlich bewirkte
   Änderung der **Dualmassen-Impulsproxies**, separat von der Körperkraft.
   Die resultierende Fluid-Körper-Impulsdifferenz wird **nicht** verborgen.

## Native Nachweise

MSVC Release und vollständige 80/80 CTest-Suites PASS.
12 analytische 3D-Konfigurationen (8³ und 12³, drei Achsen, beide
Druckgradientvorzeichen), Druck-Gauge-Invarianz, Negativfälle, unveränderte
Eingaben, unabhängig neu berechnete lokale Volumenbilanz auf jedem Voxel.
Analytischer Körperimpuls je Schritt: +/-2,56 Ns; exakt entgegengesetzter
**vorprojizierter** MAC-Gegenimpuls. Gemessene vorprojizierte
Kontinuitätsabweichung: ca. 0,008198 m³; nach Projektion maximal
ca. 8,1e-13 m³. Der unabhängige MAC-GCL-Check rechnet aus den
zurückgegebenen Face-Geschwindigkeiten und Aperturen nach.

**Wichtiger Gegenbeweis einer vollständigen FSI-Abnahme:** Die
Projektion verändert den gemessenen Dualmassen-Impulsproxy stark
(in den Referenzfällen etwa 350 Ns in der Antriebsachse). Der
resultierende Körper-/Fluid-Proxy-Impuls ist **nicht** geschlossen.
Druckkorrektur, Wandimpuls und physikalische freie-Oberflächen-/FSI-Arbeit
müssen in einem gemeinsamen konsistenten Tensor bilanziert werden.
Ein kleiner Kontinuitätsfehler beweist **keine** physikalische
Impulserhaltung. Die vorhandene Quader-Druckkraft ist eingefroren,
kein iterativ gekoppelter Flüssigkeits-/Festkörper-Drucksolve.

## Reproduzierbarkeit

`scripts/p2_moving_box_projected_reaction_full_build.cmd`:
MSVC Release Build und vollständiger CTest.
`scripts/p2_moving_box_projected_reaction_acceptance.py`:
unabhängige Zahlenkontrollen, Source-SHA-256 und CTest-Log-SHA-256.
Nachweis unter
`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_moving_box_projected_reaction_partial.json`.

## Offen

Keine gemeinsame physikalische Impulstensor- und Wandarbeitsbilanz,
kein Live-`CoupledMacWater3D`/VOF, keine freie Oberfläche, keine
globale physikalische Fluid-/Solid-Energieerhaltung, keine externen
3D-CFD-Messdaten, kein Unreal-5.8-Native-E2E für diesen Pfad.
P2–P8 bleiben PARTIAL; `ALL_COMPLETE=False`.
