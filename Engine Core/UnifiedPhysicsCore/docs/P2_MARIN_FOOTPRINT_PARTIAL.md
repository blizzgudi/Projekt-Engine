# P2 – MARIN-Drucksensor-Footprint als 3D-MAC-Teilnachweis

Stand: 01.11.2026. **P2 bleibt unvollständig; keine experimentelle Abnahme.**

## Abgegrenzter realer Entwicklungsschritt

Der neue optionale P2MarinFootprintPressure3D mittelt die **tatsächlichen nativen 3D-MAC-Zelldrücke** über eine kreisförmige Sensorfläche an den vorhandenen acht P1–P8-Positionen der MARIN-Box. Er nutzt gleichflächige Polarringe und Winkelquadratur, bilineare Interpolation in beiden Wandtangentialrichtungen und zwei flüssigkeitsseitige MAC-Druckebenen für eine lineare Normalextrapolation. Ein trockener Anteil der Sensorfläche trägt **0 Pa Gauge-Druck** bei; er wird nicht stillschweigend auf die verbleibende nasse Fläche renormiert. Nasse Flächenanteile, Extrapolations- und Fallback-Zahlen sind explizite Diagnosen.

**Der originale physische Durchmesser der P1–P8-MARIN-Transducer ist hier nicht belegt.** Der Operator verlangt deshalb einen expliziten Radius. 0,01 m in den Native-Tests ist eine **frei gewählte numerische Prüfgeometrie, keine Behauptung über die Laborhardware**. Eine andere Dammbrecherhebung mit 4,2-mm-Sensoren belegt den MARIN-Durchmesser ausdrücklich **nicht**. Für die experimentelle Auswertung muss die Original-Testbeschreibung überprüft und der tatsächliche Footprint samt Unsicherheit eingetragen werden.

Das Verfahren ändert **weder** die Poisson-/MAC-Projektion **noch** den Fluid-/Festkörper-Solver. Die normale Druckextrapolation ist ein Druck-Proxy, **keine** viskose Wandtraktion oder spannungskonsistente Wasser/Luft-Grenzbedingung.

## Reproduzierte positive und negative Native-Ergebnisse

- Win64-MSVC-C++17 Release /O2 /MD mit realem Build-Log ENGINE TEST STUBE/runs/p2_marin_footprint_build.txt; ausführbare Datei build/upc_p2_marin_footprint_tests.exe.
- **145 native Assertions PASS**: analytische affine 3D-Druckfelder bei dx=0,01/0,005 m, kreisförmige Quadratik-Flächenmomente, trockene und halb benetzte Flächen, Normalebenen-Fallback, ungültige Geometrie, NaN-Druck, NaN-Wasseranteil und ungültige Domäne.
- Echte gekoppelte 3D-MAC-PCG-Wasserläufe nach step_verified mit festem Boxhindernis und dx=0,1 m (32×10×10) sowie dx=0,05 m (64×20×20): acht benetzte Footprint-Sensoren in beiden Gittern. Native Ausgaben: P1=4424,33 Pa, P5=3335,42 Pa grob; P1=4665,39 Pa, P5=3337,35 Pa fein. Das sind **einzelne künstliche Wasser-voll-Domäne-Snapshots**, keine gemessenen MARIN-Dammbruchzeitreihen.
- Der vollständige Native-MSVC-CMake/CTest-Lauf nach Registrierung als physics_p2_marin_footprint_partial bestand mit **123/123 Tests PASS**, kein Fehler. Vollständiger Build- und CTest-Log unter ENGINE TEST STUBE/runs/p2_marin_footprint_full_build.txt und p2_marin_footprint_ctest.txt.
- Eigenständiger Python-Integritätsprüfer scripts/p2_marin_footprint_partial_acceptance.py prüft Quell-, Testbinary-, Build- und Native-Log-SHA256 und verwirft **sechs gezielt manipulierte Native-Protokolle**, darunter falschen Phasenstatus, falsche Sensor-Geometriebehauptung, doppelte Gittergröße und fehlende nasse Proben.
- Maschinenlesbarer Nachweis: ENGINE TEST STUBE/runs/p2_marin_footprint_partial.json, mit phase_complete=false, experimental_pressure_validated=false und real_sensor_radius_known=false.

## Noch nicht erreicht

Die originalen SPHERIC/MARIN-Messungen und das bekannte negative 6-s-Höhen-RMSE wurden **nicht neu berechnet oder verbessert**. Die Drucksensorfläche ist parametrisierbar, aber mangels verifiziertem Sensordurchmesser noch nicht laborgetreu kalibriert. Für die volle P2-Abnahme fehlen mindestens die echte stresskonsistente 3D-Wasser/Luft-/Wandgrenze, geometrisch und zeitlich aufgelöste P1–P8-Druckmessung, vollständige unabhängige 6-s-Dreigitter-Konvergenz H1–H4/P1–P8, bidirektionale dynamische FSI und globale physikalische Massen-/Impuls-/Energieerhaltung. Der vorhandene phase_gate.py muss deshalb **P2 PARTIAL / ALL_COMPLETE=False** melden.

Originaler Referenzdatensatz: https://www.spheric-sph.org/tests/test-02 (SPHERIC Test 02, MARIN 3D-Dammbreak). Kein unbelegter Transducer-Durchmesser wurde übernommen.
