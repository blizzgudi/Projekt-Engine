# P2 – konservativer geometrischer PLIC-Periodentransport (Teilabnahme, 31.10.2026)

**P2 bleibt INCOMPLETE.** Dieser isolierte 3D-VOF-Operator ist ein begrenzter
geometrischer Transport fuer **eine** kartesische Richtung mit raeumlich
konstanter vorgeschriebener Geschwindigkeit und periodischem Rand.
Er ist kein gekoppelter 3D-Navier-Stokes-Solver.

## Umfang

- p2_plic_outgoing_slab3d schneidet eine PLIC-Fluessigkeitshalbzelle mit
  einem ausstroemseitigen Slab von Breite Courant. Das Volumen
  wird geometrisch ueber den vorhandenen Tetraeder-Clipper berechnet,
  nicht als erstordentlicher Donor-Cell-Fluss (f * Courant).
- p2_plic_periodic_sweep3d transportiert diese Volumina ueber periodische
  Zellflaechen auf einem echten 3D-Gitter, separat fuer x, y oder z.
  Beide Nachbarzellen werden mit gleichem Betrag und entgegengesetztem
  Vorzeichen veraendert. CFL muss <=1 sein; Fehler sind atomar.
- Die Normalen werden vom Aufrufer geliefert. Die nativen Szenarien
  verwenden dafuer den vorhandenen Nachbarschafts-Rekonstruktor.
  Keine Oberflaechenspannung, bewegten Festkoerper, Mehr-Achsen-Splitting,
  Volumen-/Impuls-Kopplung, nichtperiodischen Rand oder Energieabnahme.

## Unabhaengige Referenzen

Fuer Wasser x+y<=1 in einem Einheitswuerfel und Courant 0,25 ergibt
die analytische Integration in den rechten Slab 0.25^2/2 = 0.03125
und in den linken Slab 0.25 - 0.25^2/2 = 0.21875 des Zellvolumens.

Ein dreidimensional extrudierter Fluessigkeitsblock auf 8^3 Zellen mit
zwei exakt achsnormalen PLIC-Teilzellen wird mit Courant 0,25 in x,
y und z, jeweils vorwaerts und rueckwaerts, bewegt. Nach einem und
zwei Schritten wird jede Zelle gegen die analytisch um 0,25 bzw.
0,5 Zellbreiten verschobene Grenzflaeche geprueft. Masse, Grenzen,
Richtungsumkehr und unveraenderte Eingaben sind Teil der Abnahme.

Nachweise: scripts/p2_plic_periodic_sweep_acceptance.py,
ENGINE TEST STUBE/runs/p2_plic_periodic_sweep_partial.json,
native MSVC/CTest-Buildprotokolle und SHA256 der Quell- und Binaerdateien.

**Kein Phasenabschluss.** Es fehlen freie 3D-Oberflaechenadvektion
mit nichtuniformem, gekoppeltem MAC-Feld, dynamische Zweiwege-FSI,
Wasser/Boden- und Waermekopplung, unabhaengige CFD-Messdaten,
physikalische Gesamtenergie-/Impulserhaltung und Unreal-5.8-Native-E2E.
