# P2: MARIN Test 02 — geometrisch aufgelöste Hindernisvolumina und MAC-Face-Flächen

**02.11.2026 — geometrische Teilvalidierung, P2 bleibt PARTIAL.**

## Reproduzierter Fehler im echten 3D-Versuchsaufbau

Die vorhandenen vollständigen 6-s-MARIN/SPHERIC-Test-02-Läufe verwenden eine binäre Zellmittenmaske für das Hindernis x=[0,6635;0,8245], y=[0,295;0,698], z=[0;0,161] m. Das physikalische Volumen beträgt exakt 0,010446163 m³. Der native Test `tests/p2_marin_obstacle_cut_geometry3d_tests.cpp` vergleicht die **bereits vorhandene**, analytisch geschnittene Box-/Voxel-Geometrie mit der tatsächlich im Benchmark verwendeten binären Maske. Außerdem überprüft er auf allen drei Gittern jede x-/y-/z-MAC-Face-Ebene gegen die analytische Hindernis-Querschnittsfläche.

| Gitter | analytisch / Cut-Cell m³ | binär m³ | binärer Volumenfehler | partielle Cut-Zellen | Sensor-Wirtszellen ohne binäres Hindernis |
|---|---:|---:|---:|---:|---:|
| 32×10×10, dx=0,10 m | 0,010446163 | 0,008000000 | -23,417 % | 27 | 6 von 8 |
| 48×15×15, dx=0,0666667 m | 0,010446163 | 0,007111111 | -31,926 % | 64 | 8 von 8 |
| 64×20×20, dx=0,05 m | 0,010446163 | 0,009000000 | -13,844 % | 102 | 8 von 8 |

Die maximalen analytischen Flächenresiduen über **alle** x/y/z-MAC-Face-Ebenen sind 1,39e-17 / 2,78e-17 / 2,78e-17 m². Die Cut-Cell-Volumenresiduen liegen unter 1e-14 m³. Die binären Fehler sind nicht monoton; dies ist ein konkreter, auf allen drei bestehenden Versuchsauflösungen nachgewiesener Geometrie-Blocker für korrekte Wandtraktion und Drucksensor-Auswertung. Ein Sensor-Wirtsvoxel ohne binäre Hindernisbelegung bedeutet **nicht**, dass der physische Sensor selbst fehlt, sondern dass die reale Wand im numerischen Voxel nicht als Wand repräsentiert ist.

## Reproduzierbarkeit und unabhängige QA

`scripts/p2_marin_box_geometry_native.cmd` initialisiert die echte MSVC-x64-Umgebung, baut den aktuellen C++17-Native-Test und führt die vollständige CTest-Suite aus. Die unabhängige Python-QA `scripts/p2_marin_box_geometry_audit.py` führt das Native-Executable separat erneut aus, vergleicht die Ergebnisse mit einer von C++ getrennten rational-arithmetischen Box-Volumen-/Zellmitten-/Sensorreferenz und verwirft absichtlich manipulierte Logs. Die Build-/Native-/CTest-Rohlogs und die SHA-256-Hashes sind unter `ENGINE TEST STUBE/runs/p2_marin_box_geom_*` abgelegt. Eine direkte CMake-Build-Probe ohne initialisierte MSVC-Umgebung scheiterte reproduzierbar an fehlendem C++-Standardheader; die anschließende reguläre MSVC-Build-Kette wurde korrekt initialisiert. Dieser Infrastrukturfehler ist kein physikalischer Solverfehler.

## Grenzen / negative physikalische Ergebnisse

Die exakte statische Cut-Cell-Geometrie und zeitgemittelten MAC-Face-Aperturen existierten bereits als **isolierte Bibliotheksoperatoren**; dieser Schritt liefert erstmals die quantitative Test-02-Geometrie-Diagnose und den dreigitterigen analytischen Face-Flächen-Nachweis. **Die produktive MARIN-Druck-/VOF-/MAC-Kette verwendet weiterhin die binäre Maske.** Es wurde keine physikalische Messabweichung verbessert, keine 6-s-Dreigitter-Simulation wiederholt und keine Drucktransducer-Kalibrierung vorgenommen.

Unverändert negativ: H1–H4 RMSE auf feinem Gitter 0,094788 / 0,100674 / 0,117963 / 0,137818 m gegenüber unverändertem 0,05-m-Limit; P1–P8 ca. 5,8–5,9 kPa, keine Messkonvergenz. Geometrische Cut-Cells müssen erst in den tatsächlich konservativen MAC-Dualvolumen-Transport, die variable Druckprojektion, Wanddruck-/Kraft- und Energieflüsse integriert werden; anschließend ist eine neue unabhängige vollständige Messvalidierung erforderlich. Scharfe bewegte Wasser/Luft-Spannung, dynamische bidirektionale FSI und UE5.8-E2E der vollständigen Kette sind nicht abgenommen.

**P2=PARTIAL, ALL_COMPLETE=False. Kein vollständiger Phasen-PASS.**
