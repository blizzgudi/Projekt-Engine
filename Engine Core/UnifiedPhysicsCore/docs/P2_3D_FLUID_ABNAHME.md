# P2 – 3D Fluid Solver: Entscheidung und Abnahme
Stand: 2026-10-26. Projekt: D:\Projekt Engine. Phase P2 ist ein CPU-3D-Wassersolver-MVP und eine dokumentierte Verfahrenswahl. Sie umfasst nicht die vollständigen P3–P8-Systeme.

## Verfahrensvergleich (Forschung, keine behaupteten Benchmarks aller vier Varianten)

| Verfahren | Kernmechanik | Eignung | Grenzen |
|---|---|---|---|
| MAC-Gitter + konservativer VOF-Transport | Versetzte Geschwindigkeiten, Zellendruck, Druckprojektion, Materialvolumen pro Zelle | AUSGEWÄHLT: direkt zu Voxelzuständen und gemeinsamen Material-IDs passend | Numerische Diffusion; Details unterhalb einer Zelle fehlen |
| FLIP | Flüssigkeitspartikel für Impuls-Transport, Druck/Kräfte auf Gitter | Für spätere freie Oberflächen und kleine Flüssigkeitsdetails interessant | Partikel-Gitter-Transfers und Partikelverwaltung; Rauschen |
| APIC | Affine Geschwindigkeitsfelder pro Partikel bei PIC/FLIP | Kandidat für Rotationen und reduzierte Partikelinstabilitäten | Zusätzliche Matrizen pro Partikel; komplexer |
| MPM | Materialpunkte + Hintergrundgitter für deformierbare Materie | Für P3/P5 (Sand, Boden, plastische Verformung) interessant | Konstitutive Materialmodelle und komplexere Krafteintegration |

Architekturentscheidung: MAC+VOF als erster CPU-3D-Basiskern. FLIP, APIC und MPM sind untersuchte Alternativen, NICHT implementiert oder auf dieser Hardware gegeneinander vermessen.

## Tatsächlich implementiert

- include/upc/mac3d.hpp: MAC-Geschwindigkeiten auf Zellfaces, Zellendruck, iterative Divergenz-Projektion, Schwerkraft und Wandbedingungen; Druckrandbedingung zur freien Oberfläche am Zellface.
- include/upc/coupled_water3d.hpp: ein gemeinsames 3D-Gitter mit MAC-Druck/Schwerkraft, semi-Lagrangescher Geschwindigkeitsadvektion, konservativem 3D-Volumentransport, adaptiven CFL-Teilzeitschritten, Kollisions- und Massenprüfung.
- include/upc/fluid3d_module.hpp: Public Runtime-API mit gemeinsamen Material-IDs und Dichten und 3D-Wasserdomänen.
- tests/coupled_water3d_tests.cpp: Dammbruch, durchgehende 3D-Sperrwand, Wasseranteile, Druck und Massenerhaltung.
- tests/unified_runtime_tests.cpp: gemeinsame Materialdaten und ein Simulations-Tick.
- benchmarks/p2_reference.cpp: quantitative Dammbruch- und Hydrostatenfälle bei je drei Gitterauflösungen.
- scripts/p2_acceptance.py: native Testprogramme, numerische Kriterien, Zeit/Speicher, Hashnachweis und reproduzierbares JSON.

## Quantitative Referenzfälle

Dammbruch in geschlossener Box 1,6 × 0,8 × 1,2 m, anfänglich 0,256 m³ Wasser. Bei 8×4×6, 16×8×12 und 32×16×24 Zellen bleibt das Volumen während 0,2 s Simulation innerhalb 1e-8 m³ erhalten. Der Wasserschwerpunkt bewegt sich in x-Richtung. Beispielwerte 0,224709 m / 0,219866 m / 0,215754 m. Die Differenzen beim Rasterwechsel nehmen ab (0,004843 m zu 0,004112 m). Es gibt noch keine externe experimentelle Validierung der 3D-Dammbruchfront.

Hydrostatik: Referenz am tiefsten Zellzentrum P = rho*g*(H - dz/2), rho=1000 kg/m³, g=9,81 m/s². In 8³, 16³ und 24³ Zellen liegt die relative Abweichung bei unter 1 %. Der im ersten Versuch erfasste freie-Oberflächen-Druckfehler wurde behoben: druckfreie Ghost-Fluid-Randbedingung an der Halbzellgrenze statt am Luft-Zellzentrum.

Hindernisse: Vollständige innere 3D-Sperrwand, keine Durchdringung durch Wand oder auf die abgetrennte Seite. Dies beweist Kollision mit ruhenden Hindernissen, NICHT dynamische Fluid-Festkörper-Kopplung (P3).

Speicherbudget: Rechnerische residenten Datenfelder für 8×4×6: 9.088 B, 16×8×12: 69.376 B, 32×16×24: 541.696 B. Das ist eine MODELLSCHAETZUNG ohne temporäre Buffers, Allocator/Prozess/GPU. CPU-Zeiten sind einzelne Hardwaremessungen, keine FPS. Evidenz: ENGINE TEST STUBE/runs/p2_acceptance.json.

## Grenzen und noch nicht realisierte Physik

- Wasseroberfläche aus einfachen [0,1]-Zellanteilen; druckfreie Flüssig-Luft-Grenze näherungsweise am Zellface, keine PLIC-/Level-Set-Rekonstruktion.
- Der erste-Ordnung-Volumentransport mit Kapazitäts-Limitern ist numerisch diffus und nicht hochauflösender geometrischer VOF.
- Semi-Lagrangesche Geschwindigkeitsadvektion ist dissipativ.
- Noch kein Viskositäts-, Oberflächenspannungs-, Luft-/Wasser-Mehrphasenmodell, keine subzellulären Spritzer und keine extern kalibrierte 3D-Wellenphysik.
- Keine beweglichen Festkörper, keine mehrmaterialige Impuls- und Energiekopplung (P3), keine GPU (P7), kein UE-Visual-E2E (P8).
- Der Druckprojektor ist iteratives Gauss-Seidel ohne Multigrid: skaliert für riesige Welten schlecht.

## Abnahme von P2 als Prototyp und Verfahrensentscheidung

1. C++17 MSVC Release-Build und gesamtes CTest PASS.
2. Gemeinsame Runtime-API und zentrale Materialdichte/Materialphase bei 3D-Wasser.
3. 3D-Dammbruch bei drei Rasterauflösungen und Volumenfehler < 1e-8 m³.
4. Hydrostatik bei drei Auflösungen mit Fehler < 1 %.
5. Durchgehende 3D-Hindernisse und gültige [0,1]-Wasseranteile.
6. Auflösungsvergleich, CPU-Laufzeit und residenter Datenfeld-Speicherverbrauch dokumentiert.
7. Reproduzierbares, maschinenlesbares, gehashtes Ergebnis durch scripts/p2_acceptance.py.

P2 als nachgewiesener CPU-MVP bedeutet nicht, dass der komplette Unified Physics Core fertig ist.

## Technische Referenzen
- NVIDIA GPU Gems 3, 3D Fluids: https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-30-real-time-simulation-and-rendering-3d-fluids
- Batty, Bertails & Bridson 2007, Fluid-Solid Coupling und Free Surface: https://uwspace.uwaterloo.ca/items/f5cb877f-1b56-4ff5-81d9-f7473a5e7f1a
- Jiang et al. 2015, Affine Particle-In-Cell Method: https://doi.org/10.1145/2766996
- APIC MAC Grids und Fluids, 2020: https://escholarship.org/uc/item/0vc0g86n

Diese Referenzen begründen die Architekturwahl; die lokal gemessenen Ergebnisse gehören ausschließlich zu unserem eigenen vereinfachten CPU-Prototyp.
