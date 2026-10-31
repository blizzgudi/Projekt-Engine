# P2 – geometrischer PLIC-Transport mit periodischer 3D-Querscherung (Teilabnahme, 31.10.2026)

**P2 bleibt INCOMPLETE.** Neuer isolierter CPU-Operator
`include/upc/p2_plic_sheared_periodic3d.hpp` mit vorgegebenen
kartesischen Face-Geschwindigkeiten. Gegenueber dem bisherigen
uniformen PLIC-Periodentransport koennen Geschwindigkeiten nun zwischen
parallelen Stroemungslinien variieren und ihr Vorzeichen wechseln.
Entlang einer einzelnen periodischen Stroemungslinie muss die
Face-Geschwindigkeit konstant bleiben (diskrete Divergenzfreiheit).
Daher ist dies ein **transversales Scherfeld**, kein beliebiges
3D-MAC-Geschwindigkeitsfeld und kein unsplit Navier-Stokes-Solver.

## Rechenweg und Sicherungen

- Face i ist die positive kartesische Flaeche von Zelle i. Positive
  Geschwindigkeiten transportieren von i nach i+1, negative von i+1 nach i.
- Der vorhandene PLIC-Tetraeder-Clipper integriert die ausstroemseitige
  Teilzelle geometrisch. Alle Fluessigkeitsvolumina werden in einem
  separaten Delta-Array gebucht und anschliessend gemeinsam angewandt.
- Eingaben bleiben unveraendert. Periodischer Rand, Courant <=1,
  nichtfinite Werte, ungueltige Fuellgrade, fehlende Teilzellen-Normalen,
  nichtdivergenzfreie Stroemungslinien und ungueltige Array-Groessen
  werden abgefangen; kein partieller Writeback bei Fehlern.
- Der Operator aktualisiert **nur** Fuellgrade, nicht die
  Face-Geschwindigkeiten, Druck, Energie oder Festkoerperimpulse.

## Reproduzierbare numerische Referenzen

`tests/p2_plic_sheared_periodic3d_tests.cpp` prueft x/y/z auf
8x8x8 und 12x8x6 Zellen, mit gleichzeitig positiven, negativen und
stillstehenden Stroemungslinien. Der dreizellige Fluessigkeitsblock
hat in jeder Stroemungslinie die Fuellgrade
`[0.4, 1, 1, 0.6]`. Fuer Courant c <= 0.15 gilt nach einem Schritt
analytisch: `f_2 = 0.4 - sign(v)*c` und
`f_5 = 0.6 + sign(v)*c`. Die gemessene maximale Abweichung ist
5.56e-17. Nach zwei Schritten mit **festgehaltenen achsparallelen
Normalen** wird dieselbe diskrete Referenz mit 2c bestaetigt.
Die Masse bleibt auf allen sechs Gittern bis auf Rundungsfehler erhalten.

Wenn die PLIC-Normalen nach dem ersten Schritt **neu aus den
gescherten Nachbarfuellgraden** rekonstruiert werden, bleibt die Masse
erhalten, die analytische linienweise Referenz wird jedoch
**nicht exakt** getroffen: maximale lokale Abweichung 0.0226345
auf den ersten fuenf Gittern, 0.0552724 bei 12x8x6 in z.
Das ist eine explizit dokumentierte Begrenzung der lokalen
Gradientenrekonstruktion, **kein** Nachweis geometrisch exakter
Grenzflaechenadvektion.

Native MSVC-Release-Abnahme: 102/102 CTests bestanden,
16.397 Assertions im neuen Test. Separate maschinenlesbare
Teilabnahme: `scripts/p2_plic_sheared_acceptance.py`,
`ENGINE TEST STUBE/runs/p2_plic_sheared_partial.json`.
Build-/CTest-Protokolle und SHA256 von Quellen und Binaerdatei
liegen im selben Projekt. Fehlgeschlagener erster Testlauf
(zu strenge Zweitschritt-Annahme) wurde korrigiert, nicht verschwiegen.

**Weiter offen:** variable 3D-MAC-Geschwindigkeiten mit echter
Divergenz-/Druckkopplung, konservative unsplit-Grenzflaechenadvektion,
bewegte Festkoerper und Zweiwege-FSI, unabhaengige 3D-CFD-Messdaten,
globale Energie-/Impulserhaltung und UE5.8-Native-E2E fuer diesen Pfad.
P3–P8 sind ebenfalls nicht vollstaendig abgenommen.
