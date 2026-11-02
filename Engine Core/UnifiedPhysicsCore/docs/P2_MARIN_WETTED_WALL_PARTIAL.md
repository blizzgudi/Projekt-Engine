# P2 – MARIN Test 02: geometrisch exakte zeitabhängige Benetzung der Wanddruckflächen

**02.11.2026 – ausschließlich Teilnachweis, P2 bleibt PARTIAL.**

## Reproduzierter Defekt und Korrektur

Der vorhandene MARIN-Fünfflächen-Druckkraftdiagnostiker behandelte die vollständige Quaderoberfläche als benetzt, selbst wenn die Wasseroberfläche unter der Dachkante oder innerhalb einer vertikalen Teilfläche lag. Für einen realen bodenstehenden Quader kann dies eine unphysikalische Dachlast und falsche benetzte Flächen liefern. Ein zunächst neu hinzugefügter Native-Test scheiterte reproduzierbar beim MSVC-Build mit C2039 (fehlendes `audit_wetted`).

Der neue **opt-in**-Aufruf `P2MarinBoxWallTraction3D::audit_wetted(..., free_surface_height_m)` beschneidet die seitlichen geometrisch geschnittenen Wandkacheln **vor** der Druckrekonstruktion exakt an einer vorgegebenen horizontalen Wasserlinie. Das Quaderdach wird erst bei Erreichen seiner Höhe berücksichtigt, der trockene Bodenkontakt nie. Ungültige Wasserlinien (NaN, negativ, oberhalb Tank) werden verworfen. Die alte `audit`-API und die produktive MARIN-Simulation bleiben unverändert.

## Native und unabhängige Referenzen

MSVC-Release-Native-Tests prüfen drei reale MARIN-Gitter 32×10×10, 48×15×15, 64×20×20 und je fünf Wasserlinien 0 / 0,03 / 0,103 / 0,161 / 0,55 m. Referenzen sind **unabhängig** von der C++-Kachelquadratur abgeleitete analytische Volumen-/Flächenintegrale: seitliche benetzte Fläche `2(sx+sy) min(h,sz)`, zusätzlich Dachfläche nur bei `h>=sz`; affine x-/y-Kraft `-gradient * sx * sy * min(h,sz)`; hydrostatische Dachlast `-rho*g*(h-sz)*sx*sy` nur oberhalb der Dachkante. Getestet werden ebenfalls symmetrische hydrostatische Drehmomente, volle Rückwärtskompatibilität und negative Eingaben.

Rohprotokolle, Native-Binary, Build, gesamte CTest-Suite, unabhängiger Python-Native-Rerun, manipulierte Negativtraces, SHA-256 und das unveränderte negative historische 6-s-Laborergebnis werden durch `scripts/p2_marin_wetted_wall_audit.py` in `ENGINE TEST STUBE/runs/p2_marin_wetted_wall_partial.json` dokumentiert.

## Unverändert offen

Die Wasserlinie ist **vorgegeben und horizontal**, nicht aus der bewegten 3D-VOF-Grenzfläche rekonstruiert. Der Druck muss auf der Flüssigkeitsseite physikalisch sinnvoll vorliegen; bei diskontinuierlichem Luft-/Wasserdruck ist eine gesonderte spannungskonsistente Grenzrekonstruktion nötig. Die Benetzung beeinflusst nur eine Diagnostik: **keine** Cut-Cell-MAC-Druckprojektion, kein realer Wandtransducer, keine Zweiwege-FSI und keine Energie-/Impuls-Gesamtbilanz. Die früheren 6-s-Dreigitter-Messabweichungen bleiben FAIL (H1–H4 >0,05 m; P1–P8 ca. 5,8–5,9 kPa). Kein teurer 6-s-Rerun ohne physikalisch geänderten Live-Solver, kein neuer UE5.8-E2E-Nachweis.

**P2=PARTIAL, ALL_COMPLETE=False.**
