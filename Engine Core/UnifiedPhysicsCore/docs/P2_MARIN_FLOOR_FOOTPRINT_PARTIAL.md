# P2 Teilnachweis: bodennahe MARIN-Drucksensor-Flaechenintegration (02.11.2026)

**Status: numerischer Teilfehler korrigiert. P2-Vollabnahme weiterhin FAIL/PARTIAL.**

## Reproduzierter Fehler (native, unabhaengige Hydrostatik)

Die Druck-Flaechenabtastung `P2MarinFootprintPressure3D` verwendete fuer
die tangentiale Interpolation eine auf den ersten Zellmittelpunkt
begrenzte Koordinate. Der unterste frontseitige Test-02-Sensor P1 liegt
bei `z=0.021 m`, also unter dem Mittelpunkt der untersten
`dx=0.1 m`-Zelle (`z=0.05 m`). Auch beim `dx=0.05 m`-Gitter
schneidet seine angenommene 12-mm-Diskflaeche den Bereich unter
`z=0.025 m`. Der bisherige Operator reproduzierte deshalb selbst
eine exakt lineare hydrostatische Druckverteilung nicht.

Eine separate MSVC-Release-Kompilation des urspruenglichen Headers aus
lokalem Git `2074a00` gegen den erweiterten aktuellen Test ergab
**native Exit 1**, `FAIL P2_MARIN_FOOTPRINT manufactured hydrostatic
wall disk including floor subcell assertions=51`. Das echte
Fehlerprotokoll liegt unter
`ENGINE TEST STUBE/runs/p2_floor_footprint_baseline_20261102/baseline_failure.log`
(SHA256 `B27CB7F2A2773F4F7C421E2458013B2F04F7A4B6312BBA3109CAFC5C9936E7B0`).

## Abgegrenzte Korrektur

Bei vollstaendig benetzten, nicht durch Festkoerper verdeckten
Druckstencils extrapoliert der Operator jetzt den Druck am
Domainrand mit einer einseitigen linearen Rekonstruktion. Die
**Benetzungsflaeche wird unveraendert ausschliesslich mit
nichtnegativen, geklemmten Gewichten** ermittelt. Bei Teilbenetzung,
VOF-Schnitt oder Festkoerper bleibt die vorherige sichere
Interpolation aktiv. Diese Korrektur aendert weder den
`CoupledMacWater3D`-Solver noch die feste binare Hindernisgeometrie.

Unabhaengige Referenz: `p(z)=9810*(1-z)` Pa; P1-Wandwert
`9603.99 Pa`. Der neue native Operator liefert:

| Gitterweite | P1-Druck numerisch | analytisch | Druck nur am ersten Zellmittelpunkt |
|---|---:|---:|---:|
| 0.100 m | 9603.99 Pa | 9603.99 Pa | 9319.50 Pa |
| 0.050 m | 9603.99 Pa | 9603.99 Pa | 9564.75 Pa |

Die letzte Spalte ist eine **reine Zellmittelpunkt-Referenz**,
keine gemessene historische Disk-Integral-Abweichung. Die acht
Sensoren wurden auf zwei Gittern gegen affine Hydrostatik geprueft.
Ein gesonderter 50-%-VOF-Fall bestaetigt, dass die einseitigen
Druckgewichte nicht zur unphysikalischen Benetzungsflaeche fuehren.

## Native- und unabhängige QA

- MSVC Release: Ziel `upc_p2_marin_footprint_tests` erfolgreich gebaut.
- Native Test: **164 Assertions PASS**, inklusive zweier neuer
  bodennaher hydrostatischer Referenzen und teilbenetzter Negativkontrolle.
- Gesamte CTest-Suite nach neuem Build: **151/151 PASS**.
  Dieser Working-Tree enthielt bereits einen **nicht gestagten**
  zusaetzlichen MARIN-Cut-Geometry-Test samt CMake-Eintrag.
  Diese fremden/unfertigen Aenderungen sind nicht Teil dieses Commits;
  die Testanzahl des isolierten Commits kann daher abweichen.
- `scripts/p2_marin_floor_footprint_qa.py`: **8/8** gezielt
  manipulierte Evidenzvarianten abgewiesen, darunter falscher
  Druck, falsches Gitter, fehlender zweiter Lauf, gefaelschter
  Phasen-PASS, fehlender Native-Marker und nichtfinite Referenz.
- Nachweise: `ENGINE TEST STUBE/runs/p2_marin_floor_footprint_partial.json`,
  `p2_marin_floor_footprint_qa_native.txt`,
  `p2_marin_floor_footprint_ctest_all_20261102.log` und
  `p2_floor_footprint_baseline_20261102/baseline_failure.log`.
  Die JSON-Datei haelt Source-, Binary-, Labor- und Log-SHA256.

## Verbindliche negative Messlage

Die bestehenden echten MARIN/SPHERIC-Test-02-Dreigitter-Laeufe
wurden **nicht** neu gerechnet. Ihre feinen H1-H4-RMSE bleiben
`0.094788 / 0.100674 / 0.117963 / 0.137818 m`
bei **unveraendertem** Grenzwert `<=0.05 m`.
Die acht Druck-RMSE wachsen bei Gitterverfeinerung, die
reale Transducer-Diskgroesse ist nicht belegt und die
Druckabtastung ist weiterhin ein Druckproxy ohne
geometrisch aufgeloeste Wandtraktion.

Weiterhin offen: exakte Cut-Cell-Hindernisgeometrie im Live-MAC,
real aufgeloeste Transducer-Flaeche, Wasser/Luft-Stressbedingungen,
konservative globale Massen-/Impuls-/Energie- und FSI-Bilanzen,
physikalisch konvergierte volle sechs Sekunden auf drei Gittern
und aktuelle UE5.8 Native-E2E. Ein lokaler numerischer
Hydrostatik-PASS ist **kein** MARIN-Mess-PASS.

**P2=PARTIAL, full_p2_pass=false.**
