# P2: echter 3D-MARIN-Drucksensor-Footprint im laufenden MAC/PCG-Benchmark

**Stand: 02.11.2026. Teilnachweis, keine physikalische P2-Vollabnahme.**

## Implementierung

Der bereits vorhandene native Finite-Disk-Operator
`P2MarinFootprintPressure3D::sample` ist jetzt **opt-in** an den echten
`benchmarks/p2_marin_dambreak_heights.cpp`-Zeitreihenpfad angebunden.
Der neue letzte CLI-Parameter `footprint=0.01` aktiviert einen hypothetischen
kreisförmigen Sensor mit Radius 0,010 m, 8 flächengleichen Radialringen
und 32 Winkelpunkten. Der Druck wird aus dem laufenden 3D-MAC-Druckfeld
auf der Box-Vorderseite beziehungsweise Box-Oberseite ausgelesen.

Der **tatsächliche Durchmesser der acht MARIN-Test-02-Sensoren ist damit
nicht nachgewiesen**. Das Ergebnis ist eine reproduzierbare
Sensitivitätsstudie, kein kalibriertes Transducermodell und keine
vollständige Wandspannung. Der bestehende interpolierte Punktmodus bleibt
unverändert; ein fehlerhafter Radius oder eine widersprüchliche
CLI-Kombination wird vor der Simulation abgelehnt.

Beispiel (aus dem bestehenden Projektordner, keine neue D:-Wurzel):

```cmd
build\p2_marin_dambreak_heights.exe 60 throughflow fine dense interp pcg height_interp footprint=0.01
```

## Native Mess- und Regressionsergebnisse

Der neue MSVC-Release-Benchmark wurde erfolgreich neu gebaut.
Die gesamte bestehende CTest-Suite lief **150/150 PASS**.
Für drei **wirklich verschiedene native 3D-Gitter** wurden jeweils
zwei getrennte echte 60-Schritt-Zeitreihen von 0,00 bis 0,60 s gerechnet
(Punkt- und Disk-Sampling, gleicher Fluid-Solver):

| Gitter | Zellweite | Zustände je Modus | Zeitpunkte mit benetztem Sensor | Größte absolute Disk-/Punkt-Druckdifferenz |
|---|---:|---:|---:|---:|
| 32×10×10 | 0,100 m | 61 | 32 | 16.003,09 Pa |
| 48×15×15 | 0,066667 m | 61 | 27 | 24.802,51 Pa |
| 64×20×20 | 0,050 m | 61 | 23 | 21.633,73 Pa |

Die vorhandenen **600-Schritt-/6-s-Punktdruckspuren** wurden **nicht
neu gerechnet**. Ihre ersten 61 Zustände wurden stattdessen unabhängig
mit den neuen Punktläufen verglichen: Druck, Wasservolumen und
Projektionsdivergenz stimmen überein. Der Disk-Modus verändert
Wasserhöhen, Masse und PCG-Divergenz nicht.

Die separate Python-QA prüft Header/Geometrie, 61 native Zustände,
Quell- und EXE-SHA256, alte 6-s-Baseline, echte Unterschiede bei
benetzten Sensoren und weist **6 ungültige native CLI-Aufrufe** sowie
**4 manipulierte Native-Zeitreihen** zurück.

## Negative physikalische Befunde und Grenzen

**Der sehr große Unterschied zwischen Punkt- und Disk-Druck ist kein
Validierungserfolg.** Der Disk-Operator gewichtet teilweise benetzte
Zellstencils mit der Flüssigkeitsfraktion und extrapoliert aus
zellzentrierten Druckwerten zur Wand. Das kann starke
Amplitudenunterschiede verursachen und ist noch keine geometrisch
aufgelöste Sensorbenetzung oder dynamische Wandtraktion.

Bei t=0,40 s beispielsweise liefert der grobe P1-Punktmodus
5.820,13 Pa, der hypothetische Disk-Modus aber nur 9,34 Pa;
bei mittlerem Gitter 5.382,98 Pa gegenüber 3,65 Pa und bei
feinem Gitter 4.518,42 Pa gegenüber 0,46 Pa.
Diese Ergebnisse müssen gegen echte Sensorfläche, Benetzungsdynamik,
Wandnormaldruck und Laborimpuls validiert werden, bevor ein physikalischer
Sensor-PASS zulässig ist.

**Nicht erreicht:** Vollständiger 6-s-Disk-Abgleich gegen die echten
H1–H4/P1–P8-Labormessungen, unabhängige Messkonvergenz, scharfe
Wasser-Luft-Grenzflächenspannung, bidirektionale dynamische FSI,
globale Energie-/Impulsbilanz und aktuelles UE5.8-Native-E2E.

Reproduzierbare Artefakte ausschließlich unter
`D:\Projekt Engine\ENGINE TEST STUBE\runs`:
`p2_marin_footprint_live_partial.json`,
`p2_marin_footprint_live_{coarse,medium,fine}_{point,disk}_60step.txt`,
`p2_marin_footprint_live_build_20261102.txt`,
`p2_marin_footprint_live_ctest_20261102.txt`.
Die Audit-Quelle ist `scripts/p2_marin_footprint_live_audit.py`;
der JSON-Bericht enthält Quell-, Binary-, Build-, CTest- und
Native-Spur-SHA256. **P2 bleibt PARTIAL; ALL_COMPLETE=False.**
