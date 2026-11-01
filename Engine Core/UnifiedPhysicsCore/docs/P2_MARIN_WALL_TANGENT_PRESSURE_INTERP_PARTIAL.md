# P2 Teilnachweis: tangentiale Wanddruck-Interpolation, keine CFD-Abnahme

Stand: 01.11.2026. **P2 bleibt INCOMPLETE.** Dieser Schritt verbessert
die räumliche Auswertung des *vorhandenen* nativen MAC-Druckfeldes an den
acht offiziellen MARIN/SPHERIC-Test-02-Drucksensoren. Er löst nicht das
physikalische Wandspannungs- oder Grenzflächenproblem.

## Methode und Grenzen

Vorher: ein einzelner nächstliegender flüssigkeitsseitiger Druckzellwert
je Sensor. P1/P2 und P6/P7 waren bei dx=0,10 m identisch, P3/P4 bei
dx=0,05 m. Neue, **optionale** Auswertung: pro Sensor der vorhandene
flüssigkeitsseitige Zellindex in Wandnormalenrichtung und eine
bilineare, liquid-volume-gewichtete Interpolation über die maximal vier
benachbarten Zellen in den beiden Wandtangenten. Feste und trockene
Zellen werden ausgeschlossen, verbleibende Gewichte renormiert.
Der bestehende Drucksolver und die Standard-/Legacy-Sensoren bleiben
unverändert; neuer Native-Modus ist "interp" statt "pressure".

Die Auswertung ist auf affine synthetische Druckfelder exakt
(175 Native-Assertions einschließlich Quadratik-Fehlerreduktion,
Trocken-/Festkörper-/NaN-Abwehr und echten Live-MAC-Werten).
Der quadratische tangentiale Interpolationsfehler sinkt bei
Halbierung der Zellweite in den geprüften Fällen.

**Keine subzelluläre Druckrekonstruktion:** Die Wandnormale bleibt
an der ersten Wasserzelle; weder dp/dn, die physikalische
Wanddruckrandbedingung noch Cut-Cell-Fläche, Wandtraktion,
Druckimpuls oder dynamische Zweiwege-FSI werden rekonstruiert.
Die 0,04-m-Abstände der echten Transducer sind bei dx=0,10 m
und teilweise bei dx=0,05 m weiterhin nicht unabhängig
physikalisch aufgelöst. Mathematisch unterschiedliche
Interpolationswerte sind kein Nachweis unabhängiger Messauflösung.

## Unabhängiger Vergleich mit Original-Labordaten

MSVC/Win64 Native-C++17, identische vorhandene
3D-Dammbruchphysik, 200 Schritte zu 0,01 s bis 2,00 s,
echte Wassermassenprüfung und 201 Zeilen je Gitter.
Die Auswertung prüft zusätzlich H1–H4 und die bitgenaue\nUnverändertheit aller Höhen-/Volumenzeitreihen gegenüber\nder historischen Nächstzell-Native-Auswertung (maximale\nDifferenz in beiden Gittern: exakt 0).\nDas externe Original-Experiment stammt aus dem gepinnten
MARIN/SPHERIC-Archiv (7.395 Messzeilen, P1–P8).
Die RMSE-Werte (Pa) sind gegen die *Originalmessungen*
über alle 200 Vergleichszeitpunkte berechnet, ohne
künstliche Verschiebung oder Kurvenanpassung.

| Druckkanal | dx=0,10 m Interpolation | dx=0,05 m Interpolation |
|---|---:|---:|
| P1 | 5.642 | 18.166 |
| P2 | 5.953 | 18.523 |
| P3 | 5.553 | 19.045 |
| P4 | 5.381 | 20.883 |
| P5 | 5.271 | 16.203 |
| P6 | 5.316 | 16.062 |
| P7 | 5.299 | 16.120 |
| P8 | 5.481 | 16.312 |

Die externen Höhen-RMSE (m) betragen bei dx=0,10 m:
H1 0,120670, H2 0,116936, H3 0,162034, H4 0,205611.
Bei dx=0,05 m: H1 0,103996, H2 0,076746,
H3 0,139819, H4 0,158084. **Alle acht Höhen-RMSE
verfehlen die vorab definierte 0,05-m-Toleranz.**
Die neue Druckdiagnose verändert die Fluidberechnung
nicht und repariert daher die Höhenphysik nicht.

Im Vergleich zum alten Nächstzellwert verbessert die
Interpolation **5 von 8** Kanälen auf jedem Gitter, jedoch
verschlechtert sich der RMSE von grob zu fein **an allen
acht Sensoren**. Beispiel P5 fein: alter Zellwert 26.373 Pa
RMSE, interpoliert 16.203 Pa RMSE. Auch das ist sehr weit
von einer experimentellen Druckvalidierung entfernt.
Weder bessere Kurven noch bestandene Tests belegen eine
korrekte physikalische Wandspannung.

## Native-Reproduktion und unabhängige QA

- Quellmodul: include/upc/p2_marin_wall_pressure_interp3d.hpp.
- Live-MAC-Integration: benchmarks/p2_marin_dambreak_heights.cpp,
  neuer optionaler Modus "interp"; vorheriger "pressure"-Modus bleibt.
- Native-Manufactured-Field-/Integritätstests:
  tests/p2_marin_wall_pressure_interp3d_tests.cpp.
- Native-Build und vollständiger CTest:
  scripts/p2_marin_wall_interp_build.cmd,
  115/115 CTest-Suites PASS.
- Externe Messdaten: scripts/p2_marin_wall_interp_compare.py --run,
  beide Gitter tatsächlich erneut gerechnet; keine Fakedaten.
- Manipulations-Regressionen:
  tests/p2_marin_wall_interp_regression.py, 15/15 PASS.
- Gesamt-Teilnachweis mit Quell-/Binär-/Log-Hashes:
  scripts/p2_marin_wall_interp_partial_acceptance.py und
  ENGINE TEST STUBE/runs/p2_marin_wall_interp_partial_acceptance.json.
- Messreihen und Logs im bestehenden ENGINE TEST STUBE/runs:
  p2_marin_wall_interp_coarse200.txt,
  p2_marin_wall_interp_fine200.txt,
  p2_marin_wall_interp_comparison_partial.json,
  p2_marin_wall_interp_compile.txt,
  p2_marin_wall_interp_ctest.txt.

Der alte Vergleich bleibt als historische native
Referenz dokumentiert, aber seine damals signierte
Benchmark-Quellfassung ist nicht mehr der aktuelle
Quellstand. Daher ist das alte Abnahmeprotokoll
nicht als erneuter aktueller Build-Nachweis zu verstehen.

**Offen:** Stresskonsistente Wasser-Luft-Grenzbedingungen,
Drucksolver-/Wandnormal-Konvergenz, geometrisch aufgelöste
P1–P8-Druckflächen, vollständiges >=6-s-Fenster,
Druckimpuls, dynamische Zweiwege-FSI und globale
Massen-/Impuls-/Energiebilanzen. P2 bleibt PARTIAL;
ALL_COMPLETE=False.
