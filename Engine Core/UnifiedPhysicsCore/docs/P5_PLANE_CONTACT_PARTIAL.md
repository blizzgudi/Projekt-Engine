# P5 Teilnachweis: diskreter Punktmasse-Ebene-Kontakt

Stand: 2026-10-26. **P5 bleibt PARTIAL, keine Phasenabnahme.**

## Umfang
Neue isolierte Header-Komponente `include/upc/solid_contact3d.hpp`
verwendet den bestehenden `SpringSolid3D`-Schritt und löst anschließend
Kontakt einzelner freier Massknoten mit **einer unbeweglichen, unendlichen
Ebene**. Die Ebene hat normierte beliebige Orientierung, Offset, Restitution,
Aufprallschwelle und einen begrenzten Coulomb-Reibimpuls. Durch die
Projektion wird die Halbraum-Nichtdurchdringung an Schrittenden erfüllt.
Die Impulse dissipieren bzw. erhalten die normale kinetische Energie
für 0 <= Restitution <= 1; die Reibung erhöht den Tangentialbetrag nicht.

## Nachweis
`tests/solid_contact3d_tests.cpp` deckt Ruhe unter Erdschwerkraft
(10.000 Schritte), Rückprall und Energie, Schrägstoß/Reibimpuls,
Reibungssättigung bei kleinen Geschwindigkeiten, nicht normierte und
gedrehte Ebenen, fixierte Knoten, Massenskalierung und ungültige
Parameter ab. Der neue Test ist in CMake/CTest registriert.
Verifizierter MSVC-Release-Build: neuer C++17-Test kompiliert; alle
**19/19 CTest-Suites PASS**; die P5-Tests meldeten **10.984 native Checks**
(einschließlich 480 deterministischer Energie-/Kontaktfälle mit je zwei
Assertions). Das vollständige quantitative P1–P4-Evidence wurde nach
der CMake-Registrierung neu gerechnet und die Source-Hashes erneut geprüft:
`phase_gate.py` meldet P0–P4 PASS und P5–P8 PARTIAL.

Reproduktionsbefehl nach MSVC-Build: `python scripts/p5_contact_partial.py`.
Maschinenlesbarer Nachweis: `ENGINE TEST STUBE/runs/p5_contact_partial.json`
(mit Source-Hashes, CTest-Summary und Native-Assertion-Anzahl).

## Explizite Grenzen
Kein volumetrischer oder flächenbasierter Festkörperkontakt, keine
Selbstkollision, keine Mehrfachkontakt-/Kontaktstapel-Lösung, keine
rotatorische Starrkörperdynamik, kein granuläres Material und keine
Kontinuums-FEM/MPM-Validierung. Kein kontinuierliches
Kollisionserkennen; starke Federsteifigkeit erfordert einen geeigneten
Integrator/Zeitschritt. Statische Reibung auf geneigten Ebenen kann
durch diskrete Nachprojektion Ortsdrift erzeugen. Für P5-PASS
sind robuste belastete Kontakte und Granulat-/Bruchreferenzen
zusätzlich quantitativ nachzuweisen.
