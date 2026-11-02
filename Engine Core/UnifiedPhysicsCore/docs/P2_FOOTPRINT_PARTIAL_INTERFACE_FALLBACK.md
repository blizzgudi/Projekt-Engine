# P2 – MARIN-Drucksensor: kein unzulässiger Normalgradient an teilweise nassen VOF-Stencils

Stand: 2026-11-02. **Begrenzte Fehlerkorrektur, KEINE P2-Vollabnahme.**

## Reproduzierter Fehler (RED)

Der optionale finite-Disk-Sampler `P2MarinFootprintPressure3D` extrapolierte den Druck normal zur Hinderniswand auch dann aus zwei Zellschichten, wenn die beiden Stencils nur teilweise mit Wasser gefüllt waren. Bei einer unbekannten Wasser/Luft-Schnittlage ist die so extrapolierte flüssigkeitsseitige Normalableitung nicht physikalisch aufgelöst.

Ein unabhängiger nativer Reproduktionsbau mit dem vorherigen Header aus Git `b0b8248` und dem neuen Testprogramm schlug erwartungsgemäß fehl: Exitcode **1**, `FAIL P2_MARIN_FOOTPRINT partial front VOF must not extrapolate through interface assertions=70`. Originaler Red-Build und Red-Run sind in `ENGINE TEST STUBE/runs/p2_footprint_interface_red_repro/` protokolliert. Die historische Quelldatei wurde nur in einem separaten Evidence-Verzeichnis kompiliert; der aktive Projektcode wurde dafür nicht zurückgesetzt.

Analytische synthetische Kontrollfälle bei dx=0,1 m, 50 % Zellwasserfüllung, Druckgradient 1000 Pa/m: an der Front P1 muss die erste wasserhaltige Ebene 0,5 × 1850 = **925 Pa** liefern (die alte Normalextrapolation lieferte 912,25 Pa); am Dach P5 muss 0,5 × 1250 = **625 Pa** gelten (alter Wert 580,5 Pa). Diese Zahlen sind **hergestellte Referenzen**, keine MARIN-Labormesswerte.

## Korrektur und unabhängige Kontrollen (GREEN)

In `include/upc/p2_marin_footprint_pressure3d.hpp` ist die lineare Normalextrapolation jetzt nur zulässig, wenn **beide** tangentialen Stencils vollständig nass sind (jeweils Wassergewicht mindestens 1−1e−12). Bei teilweise nassen Stencils fällt der Operator auf den liquidseitigen ersten Zellschichtwert zurück; trocken bleibt der Beitrag null. Dieser Fallback ist bewusst konservativ, **keine** neue physikalische freie-Oberflächen-Randbedingung.

Die neuen nativen Kontrollen in `tests/p2_marin_footprint_pressure3d_tests.cpp` prüfen getrennt die synthetischen Front- und Dach-Gradienten, die expliziten Fallback-Zähler und die weiterhin korrekte affine Normalextrapolation bei vollständig nassen Stencils. Die vorhandenen Tests umfassen Hydrostatik, Flächenmomente, Dry-/Half-Wet-/NaN-Negativfälle und echte 3D-MAC-Snapshots.

Nach Änderung: nativer MSVC-Release-Build **PASS**, gezielter Test **170 Assertions PASS**, vollständiger CMake-Build **PASS**, **154/154 CTest PASS**. Native Logs, Quell- und Binärhashes sowie der unveränderte frühere Dreigitter-Messreport werden im JSON-Evidence-Report `ENGINE TEST STUBE/runs/p2_footprint_partial_interface_fallback_20261102.json` referenziert. Kein erneuter teurer 3×6-s-Lauf, da diese Änderung nur den optionalen Sensor-Disk-Postprozessor betrifft.

## Negativbefunde / unveränderte Blocker

- **MARIN-Messkonvergenz weiterhin FAIL**: letzte reale sechssekündige H1–H4-Fine-RMSE 0,094788 / 0,100674 / 0,117963 / 0,137818 m, alle größer als 0,05 m. Alle acht P1–P8-Druck-RMSE steigen bei der vorhandenen Gitterverfeinerung.
- Die reale Transducer-Radius- und Zeitantwortkalibrierung fehlen. Die VOF-Wasserfraktion ersetzt **keine** geometrisch rekonstruierte benetzte Sensorfläche.
- Das Test-02-Hindernis ist im produktiven MARIN-MAC-Lauf weiterhin binär; keine echte cut-cell-Druckprojektion. Es fehlen stresskonsistente bewegte Wasser/Luft-Grenzen, globale Energie-/Impulsbilanzen, bidirektionale dynamische FSI, experimentelle Dreigitterkonvergenz und neue vollständige UE5.8-Native-E2E.
- **P2 bleibt PARTIAL**, `phase_complete=false`. Eine grüne Regression ist keine physikalische Vollabnahme. Die unveränderte 0,05-m-Grenze bleibt verbindlich.
