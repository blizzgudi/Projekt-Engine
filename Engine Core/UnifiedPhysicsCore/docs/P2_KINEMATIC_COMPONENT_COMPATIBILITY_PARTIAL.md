# P2 – Kompatibilität getrennter 3D-MAC-Flüssigkeitskomponenten (Teilnachweis)

Stand: 2026-10-27. **P2 bleibt PARTIAL, keine vollständige FSI-/CFD-Abnahme.**

## Fehlerursache und Korrektur

`KinematicWallMacProjection3D` prüfte zuvor nur den **globalen** Volumenfluss bei vollständig geschlossenen Flüssigkeitsbereichen. Zwei voneinander getrennte, vollständig versiegelte Flüssigkeitskammern können gleich große, entgegengesetzte Wandquellen besitzen. Die globale Summe ist dann null, obwohl **jede einzelne** Kammer die Neumann-Kompatibilitätsbedingung verletzt. Eine Druckprojektion kann diese beiden unvereinbaren Quellen nicht durch Umverteilung von Wasser zwischen isolierten Kammern beheben.

Vor der Druckiteration führt der Operator nun eine deterministische 6-Nachbar-Flutfüllung über benetzte Zellen aus, erkennt pro Komponente eine mögliche Wasser-Luft-Druckgrenze und prüft den diskreten Netto-Volumenfluss **jeder geschlossenen Komponente einzeln**. Bei Inkompatibilität wird `std::invalid_argument` ausgelöst, ohne das MAC-Geschwindigkeitsfeld zu verändern. Offene Komponenten bleiben lösbar. Diagnosefelder zählen offene/geschlossene Komponenten und melden den maximalen geschlossenen Nettofluss. Die vorhandene globale Flux-Überprüfung wurde durch die stärkere lokale Bedingung ersetzt.

## Reproduzierbarer nativer Test

`tests/p2_kinematic_component_compatibility3d_tests.cpp` prüft auf einem 8×7×6-MAC-Gitter mit `dx=0.08 m`, `rho=1000 kg/m³`, `dt=0.025 s` zwei getrennte Ein-Zellen-Wasserkammern links und rechts einer zusammenhängenden, zwei Voxel breiten starren Wand. Die übrigen Zellen sind Festkörper; optional werden jeweils einzelne Luftnachbarn geöffnet.

- Beide Kammern geschlossen, `v_x=+0.12 m/s`: globale Wandvolumenquelle null, **beide** Einzelkammern inkompatibel; Ablehnung vor Änderung des MAC-Felds.
- Nur linke Kammer offen, rechte geschlossen, gleiche Wandbewegung: Ablehnung trotz vorhandener freier Oberfläche in anderer Komponente.
- Beide Kammern offen, `v_x=±0.12 m/s`: jeweils zwei offene Komponenten, Divergenz null; unabhängige Ein-Zellen-Referenz `p_links=∓192 Pa`, `p_rechts=±192 Pa`, Luft-Face-Geschwindigkeiten `±0.12 m/s`.
- Eine offene/eine geschlossene sowie beide geschlossene Kammern bei ruhender Wand: kompatibel, keine künstlichen Druckkräfte oder Flüsse.

Die eigenständige Python-Abnahme `scripts/p2_kinematic_component_acceptance.py` validiert die nativen CSV-Zahlen gegen diese unabhängig berechneten Referenzen und den vollständigen CTest-Nachweis. Die Nachweise liegen unter `ENGINE TEST STUBE/runs/p2_kinematic_component_partial.json` und `p2_component_compatibility_ctest_20261027.txt`, einschließlich SHA-256 der beteiligten Quellen, des Testprogramms und des CTest-Protokolls. Native Fehlerfälle sind ausdrücklich Teil des positiven Tests.

## Begrenzung

Der Test verwendet eine **eingefrorene** Voxelmaske mit vorgegebener Normalgeschwindigkeit, nicht eine geometrisch bewegte Körpermaske. Die Ein-Zellen-Druckreferenz ist eine diskrete analytische Kontrolle, keine unabhängige gemessene 3D-Strömungsreferenz. Eine globale FSI-Impuls-/Energieerhaltung, physikalisch korrekte Wasser-Luft-Spannungen, Wasser/Boden-/Wärme-/Reaktionskopplung und Unreal-5.8-Native-E2E für diesen neuen Pfad sind **nicht** belegt. Die bereits offenen Akzeptanzkriterien P2–P8 bleiben offen.
