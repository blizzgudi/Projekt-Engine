# P2-Teilnachweis: 16^3-MAC/PLIC-Invarianz reiner Phasen (01.11.2026)

**Umfang:** Ein exakt vollstaendig mit Wasser (`VOF=1`) oder Luft (`VOF=0`)
gefuelltes periodisches 3D-Gitter bleibt unter einem diskret divergenzfreien
MAC-Feld unveraendert. Dies ist eine mathematische Invariante der passiven
VOF-Advektion, keine stresskonsistente freie-Oberflaechen-Simulation.

## Implementierung

`include/upc/p2_plic_unsplit_mac3d.hpp` erkennt exakt uniforme Endpunkte
0/1 erst **nach** der MAC-Divergenz- und Sechs-Flaechen-CFL-Pruefung.
Fuer solche reinen Phasen entfallen unnoetige geometrische Transfers,
die vorher auf 16^3-Rastern Rundungs-Teilzellen mit nicht rekonstruierbaren
Normalen erzeugen konnten. Gemischte Füllgrade, auch ein homogenes 0.5-Feld
oder eine isolierte 0.5-Zelle, werden weiterhin strikt normalrekonstruiert
und bei fehlendem Gradienten abgewiesen. Kein globales Clamping.

## Native-Evidenz

- Test: `tests/p2_plic_pure_phase16_tests.cpp` mit 16^3-Zellen,
  nichttrivialem dreikomponentigem ABC-Beltrami-Feld und konstanter
  Galilei-Translation (0.25, -0.15, 0.10) m/s.
- Direkter Unsplit-PLIC-Transport sowie zwei echte gekoppelte Schritte
  mit nichtlinearer Impulsadvektion, Viskositaet 0.003 m^2/s,
  Druckprojektion und adaptivem geometrischem PLIC.
- Vollwasser und Leergitter: exakte VOF-Paritaet, Volumen 1/0 m^3,
  null kuenstliche Fluesse, null Endpunktkorrekturen, null Massenledger.
- Beide gekoppelten Faelle: 4 PLIC-Substeps, 2 Viskositaets-Substeps,
  maximale projizierte Divergenz 1.1102230246251565e-14 s^-1.
- Fuenf Negativpruefungen: nicht-solenoidales MAC-Feld, CFL-Ueberschreitung,
  homogen unaufgeloeste 0.5-Phase, isolierte 0.5-Teilzelle und NaN-Fuellgrad.
- MSVC-C++17-Release, **37 native Assertions**, **110/110 CTest PASS**.
- Reproduktion: `scripts/p2_plic_pure_phase16_build.cmd`,
  `scripts/p2_plic_pure_phase16_acceptance.py`.
  Logs, SHA256-Quellen und binaere Artefakte unter
  `ENGINE TEST STUBE/runs/p2_plic_pure_phase16_*`.

## Ausdrueckliche Grenzen

Die vorherige **16^3-Galilei-ABC-VOF-Slab-Blockade** mit echter
Wasser-Luft-Grenzflaeche ist durch die Invarianz reiner Phasen **nicht**
behoben und nicht abgenommen. Die bestehende 8^3/10^3/12^3-Referenz bleibt
separat nachgewiesen. Es fehlen weiterhin spannungskonsistente freie
Oberflaechen, dynamische Zweiwege-FSI, externe gemessene 3D-CFD-Referenzen,
globale physikalische Energie-/Impulsvalidierung und erneute UE5.8-Native-E2E.
**P2-P8 INCOMPLETE; ALL_COMPLETE=False.**