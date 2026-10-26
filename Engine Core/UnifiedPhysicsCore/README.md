# Unified Physics Core — v0.1 CPU-Prototyp

Eigenstaendiger, **Unreal-unabhaengiger** C++17-Prototyp als erstes Engine-Modul.

## Was implementiert ist

- Zentrale Materialbibliothek mit festen IDs, Phasen, Dichte und Viskositaet (letztere bisher nur Metadatum).
- Dichtes 3D-Voxelraster mit Material-ID, Fuellanteil, Temperatur und Schadenszustand.
- Voxel-Massenberechnung aus Volumen, Fuellanteil und Dichte.
- Zentraler Simulations-Tick fuer registrierte 2D-Wasserdomaenen.
- Ein **2D-Shallow-Water-Solver** mit Schwerkraft, Tiefenfeld, Geschwindigkeit/Impuls, hydrostatischem Druck, reflektierenden Raendern und Hindernissen.
- Numerisches Finite-Volume-Verfahren (Rusanov-Flux), automatische CFL-Teilzeitschritte, Wasser-Volumenerhaltung und Regressionstests.
- Reproduzierbarer Dam-Break-Test mit CSV-Export.

## Nicht implementiert

Keine 3D-Volumenfluessigkeit, keine Kopplung von VoxelGrid und Wasser, keine turbulenten 3D-Stroemungen, keine Viskositaetskraefte, keine Oberflaechenspannung, keine Waermeleitung, keine elastischen Festkoerper, keine Stoffreaktionen, kein GPU-Solver, kein Unreal-Runtime-Plugin. VoxelGrid ist aktuell ein Datencontainer; keine sparse chunks und keine physikalische Objektkopplung. V0.1 ist deshalb **kein vollstaendiger Unified Solver**.

## Build auf dem vorhandenen Rechner

Visual Studio 2026 Build Tools, CMake (unter VS) und C++17. Der CMake-Test heisst \`physics_core_tests\`.
Projektdateien und Build-Artefakte bleiben unter \`D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\`.

## Designregel

Ein zentraler Core besitzt Materialzustand und Simulationszeit; spezialisierte numerische Verfahren bleiben eigene, testbare Module. Parameter mit unklarer Messbasis werden nicht als validierte Physik deklariert.
