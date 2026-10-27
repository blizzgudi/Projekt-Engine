# P2 Teilabnahme: atomare Konvergenzpruefung kinematischer 3D-MAC-Projektion

Stand: 2026-10-27. **Nur P2-Teilschritt, keine Vollphasenabnahme.**

## Fehler und Korrektur

Vorher schrieb `KinematicWallMacProjection3D::project` die berechneten
MAC-Face-Geschwindigkeiten nach Erreichen von `max_iterations` in den
Solver, auch wenn die resultierende 3D-Divergenz noch zu gross war.
Ein erfolgreiches Return war daher kein Konvergenznachweis.

Jetzt wird die maximale diskrete Volumendivergenz **vor jedem MAC-Writeback**
gegen die festgelegte Toleranz `1e-8 s^-1` geprueft. Bei
Nichtkonvergenz wirft der Operator `std::runtime_error` und behaelt alle
urspruenglichen Face-Geschwindigkeiten bitgenau bei. Ein spaeterer
Aufruf mit groesserem Iterationsbudget kann denselben Zustand erneut
projizieren. Die Toleranz ist eine numerische Schutzschwelle, keine
externe physikalische Fehlernorm.

## Native Regression und unabhängige Nachpruefung

`tests/p2_kinematic_projection_convergence3d_tests.cpp` prueft
8³, 12³ und 16³ Voxel, jeweils geschlossene Neumann- und offene
Dirichlet/Neumann-Fluessigkeitsgeometrie mit zentralem 2³-Festkoerper.
Die bewegte Wand hat Translation und Rotation. Pro Szene werden
zwei unzureichende Iterationsbudgets (1 und 2) absichtlich ausgefuehrt.
Beide muessen scheitern; anschliessend werden **alle drei gestaffelten
MAC-Face-Arrays elementweise exakt** mit dem Original verglichen.
Ein Lauf mit bis zu 5000 Iterationen muss danach erfolgreich
divergenzarm konvergieren und tatsaechlich neue Faces schreiben.
Der separate Nullflussfall muss mit nur einer Iteration bestehen.

Die Referenz fuer die Anzahl der Faces ist unabhaengig vom C++-Solver:
`3*(n+1)*n*n`. Das Python-Gate prueft sechs Szenarien,
beide Ablehnungen, die Anzahl aller ueberprueften Faces,
den tatsaechlichen Konvergenzwert, die Iterationsgrenzen, die
Native-Assertionzahl und die komplette CTest-Suite.

Belege: `ENGINE TEST STUBE/runs/p2_kinematic_convergence_ctest_20261027.txt`,
`ENGINE TEST STUBE/runs/p2_kinematic_convergence_partial.json`,
`scripts/p2_kinematic_convergence_acceptance.py`.
Die JSON-Datei enthaelt SHA256-Quellhashes und Binaerhash.
Der P1-Quellhash wird nach der CMake-Erweiterung mit
`scripts/p1_acceptance.py` erneut erstellt.

## Grenzen

Der Test verhindert ein stilles Veröffentlichen numerisch
unzureichender kinematischer Geschwindigkeitsfelder. Er implementiert
**keine** verlagerten Festkoerpermasken, keinen Swept-Volume-Transport,
keine bidirektionale dynamische Fluid-Festkoerper-Kopplung,
keine globale Impuls-/Energieerhaltung, keine externen
3D-CFD-Messreferenzen und keinen neuen UE5.8-Native-E2E-Lauf.
**P2 bleibt PARTIAL; P3-P8 bleiben PARTIAL.**
