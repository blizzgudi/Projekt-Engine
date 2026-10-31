# P2: Zeitabhängige vorgeschriebene MAC/PLIC-Transportintervalle (Teilnachweis)

Stand: 2026-10-31. **P2 bleibt INCOMPLETE.**

## Abgegrenzte Implementierung

`include/upc/p2_plic_temporal_schedule3d.hpp` verarbeitet eine endliche Folge von Intervallen mit jeweils eigener, für die Intervalldauer eingefrorener 3D-MAC-Face-Geschwindigkeit. Innerhalb eines Intervalls nutzt es den vorhandenen geometrischen periodischen PLIC-Transport mit adaptivem Sechs-Face-CFL-Subcycling und erneuter Normalenrekonstruktion. Zwischen Intervallen wird das Geschwindigkeitsfeld tatsächlich ausgetauscht. Der Rückgabewert enthält Zeitsumme, Teilstepanzahlen, Transportstatistik, Divergenzmaximum und Wasserbilanz. Fehler brechen die Berechnung ab; die Eingaben werden nie geändert.

Die Grenzen sind absichtlich eng: periodische Ränder, vorgegebene und diskret divergenzfreie Geschwindigkeiten, keine Berechnung der Navier-Stokes-Impulsentwicklung, kein bewegter Festkörper, keine Zweiwege-FSI, keine physikalische globale Energie-/Impulsabnahme. Die gleichzeitigen geometrischen Fluss-Slabs bleiben eine Approximation an Ecken.

## Native Tests und Reproduktion

- Ein einzelnes Intervall ist bitgleich zum bestehenden adaptiven Operator.
- Zwei Intervalle mit unterschiedlichen Geschwindigkeiten werden gegen zwei **unabhängig aufgerufene** adaptive Schritte geprüft; Wasser bleibt auf dem 3D-Periodengitter konserviert.
- Die analytische Netto-Translation eines periodischen Wasserblocks wird anhand exakter Zell-Quader-Schnittvolumina verglichen (Fehlergrenze 12 %, keine Behauptung geometrischer Exaktheit).
- Zwei zeitlich unterschiedliche, diskret divergenzfreie MAC-Curl-Felder: gleiche Ausgabe wie manuell sequenzierte Teilaufrufe, kontrollierte CFL und Massenbilanz.
- Negative Fälle: leere Intervalle, nichtfinite Dauer/Faces, fehlerhafte Gittergröße, nichtsolenoidales zweites Intervall, erschöpftes Gesamt-Substepbudget und Zeitüberlauf.

Build: `scripts/p2_plic_temporal_build.cmd`; quantitative Prüfung: `scripts/p2_plic_temporal_acceptance.py`; Nachweise unter `ENGINE TEST STUBE/runs/p2_plic_temporal_*`. Die tatsächlichen Messergebnisse und Quell-SHA256 werden ausschließlich durch das ausgeführte Gate protokolliert.

## Offene Vollkriterien

P2: stresskonsistente freie Oberflächen, dynamische Festkörper mit Impulsrückgabe, unabhängige gemessene 3D-CFD-Referenzen, vollständige gekoppelte Energie-/Impulserhaltung. P3–P8 bleiben Vollphasen-offen, insbesondere GPU/LOD-Integration und Unreal-5.8-Native-E2E.

## Gemessene Abnahme am 31.10.2026

MSVC Release-Build und **106/106 CTest-Suites PASS**; neuer Native-Test **1503 Assertions PASS**; maschinenlesbares Teil-Gate **24/24 PASS** mit neun Quell-SHA256-Fingerprints sowie Binär-, Configure-, Build- und CTest-Protokollhashes.

- Zeitlich variable periodische X-Translation (12×10×8): 2 Intervalle / 3 Substeps; 0,018000000000000006 m³ Anfangs- und Endvolumen; analytischer relativer L1-Fehler 0,0017304825534916637; maximale Substep-CFL-Summe 0,4.
- Zeitlich veränderliches solenoidales 3D-Curl-Feld (8³): 2 Intervalle / 4 Substeps; Anfang 0,064000000000000015 m³, Ende 0,063999999999999987 m³; maximale Substep-CFL-Summe 0,22627416997969524.
- Ein anfänglicher Negativbefund durch geometrisch überlappende Ausfluss-Slabs bei umgekehrtem Wirbelfeld wurde nicht als Erfolg gewertet. Der Nachweisfall verwendet anschließend ein zulässiges, zeitlich schwächeres gleichgerichtetes Curl-Feld. Der Operator verwirft weiterhin echte Überlappungen; diese Einschränkung bleibt bestehen.

Diese Zahlen belegen nur den angegebenen vorgeschriebenen Transport. **Keine vollständige P2-Abnahme.**
