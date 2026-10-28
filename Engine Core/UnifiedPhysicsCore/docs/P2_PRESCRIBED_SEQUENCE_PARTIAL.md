# P2 Teilabnahme: zeitaufgeloeste vorgeschriebene Box-MAC/VOF-Schritte (28.10.2026)

Neue optionale `P2PrescribedBoxMacVof3D` verbindet die vorhandene
Cut-Cell-Druckprojektion mit `FractionalBoundaryVofFlux3D` ueber
**mehrere aufeinanderfolgende Zeitschritte**. Sie speichert die jeweils
aktuelle Boxgeometrie, die fraktionalen offenen Zellvolumina und die
Wasseranteile. Der MAC-Praediktor wird pro Schritt vom Aufrufer geliefert.
Die Projektion ermittelt zeitintegrierte Fluid-Face-Volumina; fuer VOF
werden diese in aperturgewichtete effektive Face-Geschwindigkeiten
umgerechnet. Das ist absichtlich **nicht** die physikalische, durch
Apertur dividierte MAC-Geschwindigkeit.

Transaktional: Wenn die Druckprojektion, die Geometrie-CFL, die
VOF-Donor-CFL oder eine Massen-/GCL-Pruefung scheitert, bleiben
Boxposition, Wasser, offene Volumina und Schrittzaehler unveraendert.
Die vorhandenen Solverpfade bleiben unangetastet.

Abnahme: `tests/p2_prescribed_box_mac_vof3d_tests.cpp` prueft 8^3,
12^3 und 16^3, alle drei Bewegungsachsen und beide Vorzeichen,
50 % und 100 % Saettigung, jeweils zwei aufeinanderfolgende
Bewegungsschritte mit synthetischem 3D-MAC-Gradient/Curl-Praediktor.
Unabhaengige analytische Referenz fuer den Wasserinhalt:
`saturation * (n^3 - 1.5^3) m^3` bei dx=1 m. Geprueft werden
Wassermasse, uniforme Saettigung, lokale GCL, Druckkontinuitaet
und Rollback bei sechs Fehlertypen. Maschinenlesbare Teilabnahme:
`scripts/p2_prescribed_sequence_acceptance.py` und
`ENGINE TEST STUBE/runs/p2_prescribed_sequence_partial.json`.
Vollstaendiges CTest-Protokoll im gleichen runs-Ordner.

**P2 bleibt PARTIAL.** Keine Einbindung in den allgemeinen
`CoupledMacWater3D`-Live-Solver, keine zeitliche Impulsadvektion
oder Praediktor-Rueckfuehrung, keine freie Oberflaeche, keine
dynamische Zweiwege-FSI, keine Gesamtenergie-/Impulsabnahme,
keine externen 3D-CFD-Messdaten und keine neue UE5.8-Native-E2E.
P3-P8 bleiben Vollphasen-offen.

Verifizierte Ausfuehrung am 28.10.2026: MSVC Release Build PASS;
62/62 CTest-Suites PASS (vollstaendige Regression), 304651 native
Assertions im neuen sequentiellen Test, 288/288 unabhaengige numerische
Python-Pruefungen PASS. P1-Referenzabnahme 33/33 erneuert.
Die SHA-256-Fingerprints im Teilabnahme-JSON binden Code, Test,
Abnahmeskript, Dokumentation und CMakeLists.txt an diese Ausfuehrung.
