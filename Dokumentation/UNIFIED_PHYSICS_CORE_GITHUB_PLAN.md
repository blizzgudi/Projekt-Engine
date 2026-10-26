# Unified Physics Core — GitHub-Plan
Stand: 2026-10-26

## Repository
Git-Root: D:\Projekt Engine
Gewuenschter Remote: separates privates Repository `blizzgudi/Projekt-Engine`
Nicht identisch mit dem bestehenden privaten Repository `blizzgudi/AI-Game-Studio`.
GitHub Remote erst nach nachgewiesener Anlage eintragen. Kein Blind-Push und keine Freigabe von Spiel- oder Projektdateien.

## Geplante GitHub-Issues
- P0 — Baufaehiger C++17-Core und Regressionstests (Material-IDs, Voxelzustand, Git).
- P1 — 2D-Wassersolver / Dammbruch-Validierung gegen Referenzprofile, CFL, Masse, Druck, Hindernisse.
- P2 — 3D-Solver-Vergleich MAC/FLIP/APIC/MPM; Mini-Benchmark und Auswahlbegruendung.
- P3 — Gemeinsame Voxel-Fluid-Kopplung mit poroesem Boden (Masse/Impuls).
- P4 — Waermebilanz, Gefrieren und Verdampfen.
- P5 — Elastizitaet, Verformung, granulare Stoffe und Bruch.
- P6 — Materialmischungen/Umwandlungen mit validierter Stoffbilanz.
- P7 — Sparse Chunks, LOD und CPU-/GPU-Performance.
- P8 — UE5.8-Plugin-Adapter, Runtime-Beweis und visueller Test.

## Definition of Done je Issue
Compiler sauber, Tests bestanden, Referenzwerte/Fehlergrenzen, dokumentierte Grenzen,
reproduzierbares Artefakt und ein klarer PASS/FAIL/NOT-RUN-Nachweis.

## Architekturprinzip
Ein zentraler Simulations-API und gemeinsame Material-IDs. Numerische Solver duerfen
intern spezialisiert bleiben und werden nicht als unrealistische Universalformel verkauft.

## Status
Die Issues werden erst nach realer GitHub-Repo-Erstellung dort erstellt.
Bis dahin ist dies die verbindliche lokale Phasenaufteilung.

## Nachweis v0.1
- Stand: C++17, MSVC 19.51, CMake/Ninja Release-Build erfolgreich.
- CTest: 1/1 PASS; direkter Test: 472 Assertions PASS.
- Dammbruch-Test: 41 CSV-Zeilen, 1.56 m3 Anfang/Ende, ca. 1560 kg Wasser, rechte Wassertiefe am Ende ca. 0.221089 m.
- Lokaler Git-Commit: 7f15ef8 auf Branch main.
- GitHub-Remote: NICHT ANGELEGT / NICHT GEPUSHT. Keine GitHub-CI-Ausfuehrung behaupten.
