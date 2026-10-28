# P2 Teilnachweis: analytische 3D-Cut-Cell-Geometrie eines bewegten Quaders

Stand: 2026-10-28. **P2 bleibt PARTIAL; P3-P8 bleiben PARTIAL.**

## Abgegrenzte Implementierung

`include/upc/p2_moving_box_open_volume3d.hpp` berechnet aus den realen
Koordinaten eines starren achsparallelen Quaders vor und nach einer Translation
die exakten Schnittvolumina mit jeder 3D-Voxelzelle. Ausgabe sind die
fluidzugaenglichen Volumenanteile `alpha_old` und `alpha_new` im Intervall
[0,1]. Das ersetzt fuer diesen Geometriepfad manuell vorgegebene Alpha-Werte.
Eine Vollzelle hat Volumen `dx^3`. Der geometrische Schnitt ist das Produkt
der drei geklippten eindimensionalen Intervallueberlappungen.

Voraussetzungen: kartesisches Gitter mit Ursprung (0,0,0), starrer
achsparalleler Quader ohne Rotation/Deformation, vollstaendig innerhalb der
Domain, positive SI-Einheiten, Translation je Achse maximal eine Zelle pro
Zeitschritt. Unzulaessige Daten werden ohne Aenderung der Eingaben abgelehnt.
Das Ergebnis bilanziert das geometrische Festkoerpervolumen und die
fluidzugaenglichen Volumina beider Zeitpunkte.

## Referenzfaelle und Grenzen

Native Tests `tests/p2_moving_box_open_volume3d_tests.cpp`:
3 Raster (8^3, 12^3, 16^3), 3 Achsen, 2 Richtungen und 2
subzelluläre Verschiebungen (0,25 und 0,5 Zellbreiten) ergeben 36
analytische Translationsfaelle. Ein Quader mit 2x1x1 Vollzellen hat
16 m^3 bei dx=2 m. Die erwartete L1-Aenderung offener Zellvolumina
ist `2 * Querschnitt_in_Zellen * Verschiebung_in_Zellen`.
Weitere 3 nicht-gitterausgerichtete Quader pruefen explizit die
analytischen Zellueberlappungen 0,09375 / 0,0625 / 0,375
(als Festkoerperanteile). Negativfaelle: Deformation, Grenzuebertritt,
CFL, NaN, Nullschritt, Nullraster, zu grosse Raster. Das
unabhaengige Python-Gate `scripts/p2_moving_box_geometry_acceptance.py`
prueft native CSV-Zeilen gegen separat berechnete Formeln, SHA-256,
vollstaendige CTest-Regression und `phase_complete=false`.

Artefakte: `ENGINE TEST STUBE/runs/p2_box_geometry_full_ctest_20261028.txt`
und `ENGINE TEST STUBE/runs/p2_moving_box_geometry_partial.json`.

**Nicht implementiert:** Die Schnittvolumina sind KEINE swept
MAC-Face-Fluesse. Der Quader kann Fluid nur ueber eine echte
Geschwindigkeits-/Druckprojektion und geeignete freie Wege
verdrängen. Diese Komponente erzwingt weder die lokale GCL noch
berechnet sie Druck, Fluid-Gegenimpuls, Gesamtenergie, rotierende
Oberflaechen oder eine dynamische FSI-Zeitintegration.
Keine externe 3D-CFD-Messvalidierung und kein neues Unreal-5.8
Native-E2E. Sie kann erst nach einer physikalischen Flussloesung
als geometrische Eingabe fuer `FractionalBoundaryVofFlux3D` dienen.

Naechster Schritt: Geometrische Oberflaechenbewegung und
MAC-Face-Volumenfluesse mit Druckprojektion konsistent koppeln,
anschliessend Fluid-/Koerperimpuls und Energie quantitativ bilanzieren.
