# P2 Teilprüfung: acht reale MARIN-Druckmessstellen, zwei echte 3D-Gitter

Stand: 01.11.2026. **Kein P2-PASS.** Das Ergebnis ist ein physikalisch
negativer, aber reproduzierbarer Benchmark. Die numerische Rechen- und
Messkette existiert; die tatsächliche Druckvalidierung ist **gescheitert**.

## Referenz, Sensor-Positionen und Einschränkung

Die SPHERIC-/ERCOFTAC-Test-02-Dokumentation, Abbildung 7, definiert acht
Drucksensoren auf einer festen 0,161-m-hohen Hindernisbox. Die vier
Frontsensoren P1–P4 liegen bei z=0,021 / 0,061 / 0,101 / 0,141 m.
Vier Dachtransducer P5–P8 haben dieselben Abstände von der angeströmten
Boxkante entlang x. Sensoren liegen in der Breite etwa bei 0,176 m
von einer Boxkante. Die vorhandene vereinfachte Hindernisgeometrie des
Versuchsmodells ist x=0,6635–0,8245 m, y=0,295–0,698 m,
z=0–0,161 m.

Neue Quelle: include/upc/p2_marin_pressure_sensors3d.hpp.
Sie liest **echte Druckwerte des aktiven MAC-Druckprojektors** auf
der ersten flüssigkeitsseitigen Zellmitte, ohne künstliche Werte oder
frei erfundene Hydrostatik. Der Druck wird über 201 echte
Simulationszustände je Raster ausgegeben; der aktuelle Wasserkontakt
an den acht Zellpositionen wird pro Zeitpunkt separat protokolliert.

**Wesentliche Grenze:** Diese Zellmitten sind nicht identisch mit den
realen Transducer-Oberflächen; die Binär-Hindernisgeometrie ist
nicht exakt. Das 0,10-m-Gitter legt P1 und P2 sowie P6 und P7 auf
identische Zellen. Selbst bei 0,05 m fallen P3 und P4 zusammen.
Daraus folgt: keine vollständige, unabhängige räumliche Auflösung
aller acht Drucksensoren.

## Echte Native-Läufe und unabhängige Messauswertung

Beide C++17/Win64-Native-Läufe nutzen exakt den
bestehenden 3D-CoupledMacWater3D-Algorithmus mit
Schwerkraft, VOF-Wassertransport, Hindernis und
Druckprojektion, jeweils 200 Schritte bis 2,00 s.

Referenz: Offizielles MARIN/SPHERIC-Archiv, Original-XLS mit
7.395 Messreihen, davon P1–P8 in Pascal.
Die Daten werden anhand der gepinnten Quell-SHA256-Hashes validiert.
Die Messwerte sind auf alle 200 numerischen Zeiten interpoliert.
Keine künstliche Verzögerung, Offset-Korrektur oder Anpassung
der Referenzwerte an das Modell.

| Druckstelle | RMSE 0,10-m-Gitter (Pa) | RMSE 0,05-m-Gitter (Pa) |
|---|---:|---:|
| P1 | 5.646 | 18.178 |
| P2 | 6.025 | 18.691 |
| P3 | 5.053 | 20.274 |
| P4 | 5.274 | 20.342 |
| P5 | 5.254 | 26.373 |
| P6 | 5.345 | 16.060 |
| P7 | 5.317 | 16.108 |
| P8 | 5.736 | 16.356 |

**Bei allen acht Druckstellen wird der Messfehler mit höherer
Auflösung deutlich größer.** Insbesondere erreicht die
feine Dachbox-Näherung vielfach künstlich hohe Spitzen.
Das weist auf fehlende physikalische/numerische Druckkonvergenz,
unzureichende Grenzbedingungen und nicht auf eine erfolgreiche
Verfeinerung hin. Die früheren **Höhen-RMSE** verbessern sich
auf demselben feineren Gitter; daraus kann man keine korrekten
Druckkräfte oder Impulse ableiten.

## Wiederholbare technische Belege

- Native Messfunktion: include/upc/p2_marin_pressure_sensors3d.hpp.
- Echter Lauf: benchmarks/p2_marin_dambreak_heights.cpp mit
  Zusatzoption pressure, ohne Veränderung des bisherigen Standardformats.
- C++-Regressionsprüfung: tests/p2_marin_pressure_sensors3d_tests.cpp
  (184 Assertions), registriert in CMake/CTest.
- Originalmesswerte und 2-Gitter-RMSE:
  scripts/p2_marin_pressure_compare.py, Test
  tests/p2_marin_pressure_compare_regression.py mit 11 positiven
  und manipulativen Negativfällen.
- Vollständige Reproduktion und SHA-Verifikation:
  scripts/p2_marin_pressure_partial_acceptance.py,
  optional --rebuild-and-rerun für beide Native-Berechnungen.
- Protokolle ausschließlich im bestehenden
  ENGINE TEST STUBE/runs:
  p2_marin_pressure_coarse200_dense.txt,
  p2_marin_pressure_fine200_dense.txt,
  p2_marin_pressure_coarse_measured_partial.json,
  p2_marin_pressure_partial_acceptance.json und CTest-/Regression-Logs.

Das ist eine überprüfbare **Teilprüfung von Laufzeit und Datenkette**.
Keine P2-Vollabnahme. Erforderlich sind weiterhin korrekt aufgelöste
Grenzflächen und Transducerpositionen, ein 6-s-Lauf, quantitative
Druckverläufe und Impuls auf die Box, nachgewiesene Spannungsbedingungen,
globale Energie-/Impulsbilanz und dynamische Zweiwege-FSI. P2
bleibt PARTIAL. ALL_COMPLETE=False.
