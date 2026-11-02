# P2 – MARIN Test 02: geometrisch korrekte Wanddruckkraft am bodenstehenden Hindernis

**02.11.2026 – analytischer und nativer Teilnachweis. P2 bleibt PARTIAL.**

## Reproduzierter Fehler und abgegrenzte Korrektur

Der MARIN/SPHERIC-Test-02-Quader liegt mit seiner Unterseite bei `z=0` auf dem Tankboden. Der bisherige generische Operator `P2MovingBoxPressureTraction3D::audit` erwartet einen vollständig von Fluid umgebenen Quader und verwirft den Fall, weil er eine Druckzelle **unterhalb des Tankbodens** abfragt. Seine nächstgelegene Druckzelle ist außerdem bei einem nicht gitterkonformen Quader kein korrekt interpolierter Wanddruck. Eine scheinbar geschlossene sechste Fluidfläche würde eine nicht existierende Auftriebskraft am Bodenkontakt erzeugen.

Neu ist `P2MarinBoxWallTraction3D::audit` mit **fünf für Fluid exponierten Seiten** (die ausgegebene Fläche ist geometrisch und keine zeitabhängig ermittelte Wasserbenetzung), explizit ausgeschlossener trockener Boden-Kontaktfläche, exakten geometrischen Teilflächen auf jeder der drei realen MARIN-Gitterauflösungen und einer linearen Rekonstruktion aus zwei **außerhalb des Hindernisses liegenden** Flüssigkeits-Zellzentren in Wandnormalenrichtung. Tangential wird der Druck auf die Teilflächenmitte bilinear rekonstruiert. Das liefert aus beliebig orientierten linearen Druckfeldern die korrekte integrierte Kraft. Es wird weder eine Sensorfläche angenommen noch ein fehlender Druckwert im Fluid erfunden.

## Native Tests und unabhängige Referenzen

`tests/p2_marin_box_wall_traction3d_tests.cpp` verwendet dieselbe reale Quadergeometrie x=[0,6635;0,8245], y=[0,295;0,698], z=[0;0,161] m wie die unveränderten Labor-Benchmarks. Die drei Gitter 32×10×10, 48×15×15 und 64×20×20 werden unabhängig mit rational-arithmetischen analytischen Kraft- und Flächenwerten in `scripts/p2_marin_wall_traction_audit.py` geprüft. Der Test kontrolliert die exakte hydrostatische Dachlast, die drei Kraftkomponenten eines räumlich affinen Druckfeldes, verschwindende hydrostatische Drehmomente, die trockene Bodenfläche und die Unabhängigkeit von vergifteten Druckwerten in binären Solid-Zellen. Außerdem werden unzulässige Geometrien und nichtfinite Eingaben verworfen.

Ein kurzer echter `CoupledMacWater3D`-PCG-Native-Schritt speist den neuen Kraftdiagnostiker mit tatsächlichen MAC-Druckwerten. Er ist **keine** experimentelle Fluid-Festkörper-Kopplung. Build, vollständige CTest-Suite, native Rohmessung, eigenständige Binär-Neuausführung, negative Manipulationsprüfungen und SHA-256-Hashes liegen unter `ENGINE TEST STUBE/runs/p2_marin_wall_traction_*`. Die maschinenlesbare Teil-Evidence ist `p2_marin_wall_traction_partial.json`.

## Explizit nicht abgenommen

Die echte MARIN-Produktionssimulation nutzt weiterhin die **binäre** Hindernismaske; ihre Druckprojektion, der konservative MAC-Impulstransport und die bewegte Fluid-Festkörper-Grenze wurden in diesem Schritt **nicht** ersetzt. Die neue Wandkraft wird noch nicht auf einen dynamischen Körper oder als Gegenimpuls auf das Fluid angewandt. Die fünfseitige Wandlast aus einer vorgegebenen Druckverteilung ist eine korrekte **diagnostische Integration**, aber kein physikalisch vollständiger Druckrandwertsolver und kein echter Nachweis von FSI oder freier Oberflächenspannung.

Die bisherigen vollständigen sechssekündigen drei-Gitter-Messfehler bleiben ausdrücklich **FAIL**: H1–H4 auf dem feinen Gitter 0,094788 / 0,100674 / 0,117963 / 0,137818 m statt höchstens 0,05 m; P1–P8 rund 5,8–5,9 kPa. Keine kostspielige Wiederholung ohne geänderten Live-CFD-Solver. Die unabhängige Labor-Messkonvergenz, dynamische bidirektionale FSI, globale physikalische Bilanzen und vollständige UE5.8-Native-E2E bleiben offen.

**P2=PARTIAL, ALL_COMPLETE=False.**
