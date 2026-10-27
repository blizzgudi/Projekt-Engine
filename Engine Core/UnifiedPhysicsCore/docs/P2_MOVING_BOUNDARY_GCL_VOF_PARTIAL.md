# P2 Teilnachweis: bewegliche Voxelgrenze mit diskreter Volumenerhaltung

Stand: 2026-10-28. **P2 bleibt PARTIAL; keine Vollphasenabnahme.**

## Umfang

Die neue isolierte C++-Komponente
`include/upc/p2_moving_boundary_vof_flux3d.hpp` verarbeitet eine alte
und neue binaere 3D-Koerpermaske, eine unveraenderte feste Hindernismaske,
alte Wasserfuellgrade und extern vorgegebene 3D-MAC-Face-Geschwindigkeiten.
Der Operator berechnet den expliziten, lokalen Donor-Cell-Transport von
Wasservolumen ueber unmittelbar benachbarte Faces. Es gibt keinen
direkten Wassertransfer durch belegte Koerperzellen.

Die geometrische Erhaltungsgleichung wird fuer **jede** Zelle geprueft:

`alpha_neu = alpha_alt - (dt/dx) * Summe(v_face * n_face)`

mit `alpha=1` fuer frei verfuegbares Fluidvolumen und `alpha=0`
fuer Festkoerper. Das Wasserinventar `alpha*VOF` wird ueber dieselben
Face-Volumenfluesse konservativ transportiert. Jede Donorzelle darf
hoechstens ihr altes Fluidvolumen abgeben (CFL <= 1). Externe Grenzen
und fixe Hindernisse sind undurchlaessig. Wasser in neuen Festkoerperzellen
wird verworfen, nicht nachtraeglich durch Clipping gerettet. Ungueltige
Felder werden mit Fehler abgelehnt; alle Eingaben bleiben unveraendert.

**Entscheidende Grenze:** Die MAC-Face-Geschwindigkeiten werden nicht aus
der Bewegung der Maske hergeleitet. Sie muessen als physikalisch geeignete
zeitintegrierte Volumenfluesse extern berechnet und auf die lokale
geometrische Erhaltung projiziert werden. Eine diskrete GCL-Pruefung
beweist **nicht**, dass diese Fluesse durch Druck, Impuls oder Energie
physikalisch korrekt sind. Binaere Zellbewegung in einem einzigen
Zeitschritt ist weiterhin grob; kein Subvoxel- oder Kontinuums-FSI.

## Unabhaengige numerische Referenzen

- Einzeller-Translation um genau eine Gitterzelle in X/Y/Z, je beide
  Vorzeichen, je zwei seitliche Umgehungsachsen und 8^3/12^3/16^3.
  Externe vierzellige Umgehung mit drei MAC-Faces: (0.6, 0.2, 0.8)
  werden simultan zu (0, 0.6, 0.2, 0.8) umgeordnet; die vacierte
  Zelle nimmt 0.8 auf, die neue Koerperzelle bleibt trocken.
- Zweizelliger zusammenhaengender Koerper: fuenfzellige aeussere
  Route mit vier Face-Fluessen, alte/neue Maskenueberlappung bleibt
  trocken; Wasserwerte 0.9/0.1/0.3/0.5 werden konservativ verschoben.
- Statischer 3D-MAC-Zirkulationsfall als Rueckwaertskompatibilitaet:
  bei Courant 0.25 werden (1,0,0,0) zu (0.75,0.25,0,0).
- Negativtests fuer fehlenden oder ueberhoehte gesweepte Fluesse,
  unzulaessigen Donor/Empfaenger, fixe Hindernisse, Domain-Rand,
  NaN, Wasser in altem Solid, geaenderte Koerperzellzahl,
  MAC-Arraylaenge und Null-Zeitschritt.

Native Referenz: `build/upc_p2_moving_boundary_vof_flux_tests.exe`.
Vollstaendige Suite: `ctest --test-dir build -C Release --output-on-failure`.
Unabhaengiges Python-Gate:
`scripts/p2_moving_boundary_vof_acceptance.py`.
Belege: `ENGINE TEST STUBE/runs/p2_moving_boundary_vof_partial.json`
und `ENGINE TEST STUBE/runs/p2_moving_boundary_vof_full_ctest_20261028.txt`.
Source-SHA256 und die native Ausgabe sind im JSON gesichert.

## Nicht nachgewiesen

Keine automatisch aus der Koerperkinematik erzeugte
MAC-Geschwindigkeit, keine Druckprojektion nach der Maskenbewegung,
kein Fluid-Gegenimpuls, keine geschlossene mechanische Energie,
keine realen 3D-CFD-Messdaten, keine Einbindung in den Live-Solver,
kein neues Unreal-5.8-Rendering-Native-E2E. P3 bis P8 ebenfalls offen.

Naechster Schritt: physikalisch konsistente gesweepte
Grenzvolumenfluesse aus einer subzellgenauen Koerperkinematik erzeugen,
Druck/VOF/Impuls gemeinsam fortschreiben und gegen unabhaengige
3D-Referenzdaten validieren.
