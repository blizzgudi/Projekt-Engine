# P2-Teilnachweis: 3D-SDF-Kruemmung und Young-Laplace-Druck an MAC-PCG (01.11.2026)

## Neuer, real implementierter Baustein

`include/upc/p2_sdf_capillary3d.hpp` berechnet aus einer **extern bereitgestellten** zellzentrierten 3D-Signed-Distance-Funktion (negativ = Wasser, positiv = Luft) die Oberflaechenkruemmung `div(grad(phi)/|grad(phi)|)` mit dreidimensionalen zweiten und gemischten Ableitungen. Daraus entsteht fuer jede geschnittene MAC-Grenzflaeche die Druckbedingung `p_wasser = p_luft + sigma * kappa` (SI: Pa, N/m, 1/m). Der geometrische Nasszell-Abstand `theta` stammt aus der SDF-Nullstelle und wird zusammen mit dem Druck in die **bestehende native verifizierte 3D-MAC-PCG-Druckprojektion** eingespeist. Die Uebernahme ist transaktional; unpassende Wet-/Solid-Masken, nichtfinite Felder, degenerierte Schnitte und unaufgeloeste Randnormalen werden abgewiesen. Die MAC-Klasse stellt dafuer nur lesende Form-/Masken-Getter bereit.

**Physikalische Grenze:** Der neue Baustein loest weder die SDF aus der laufenden VOF-Advektion noch die tangentiale/viskose Zwei-Phasen-Spannung oder die dynamische Luft. Eine lokal berechnete Kruemmung allein ist **keine** stresskonsistente 3D-Wasser-Luft-Grenze und **keine** P2-Vollabnahme.

## Native MSVC-Release-Ergebnisse

Reproduktionsbefehl: `scripts/p2_sdf_capillary_build.cmd`. Der echte Build und das native Testprogramm wurden auf Windows ausgefuehrt; die vollstaendige bestehende Regression hat **128/128 CTest PASS** ergeben. Der neue Native-Test meldet **9.142 Assertions PASS**. Die folgenden unabhaengig analytisch berechneten Kruemmungen werden gegen den nativen 3D-Stencil verglichen (Radius R = 0,287 m):

| Geometrie | dx (m) | Geschnittene Faces | Analytisch (1/m) | Numerisches Mittel (1/m) | Max. relativer Fehler |
|---|---:|---:|---:|---:|---:|
| Kugel, 2/R | 0,08 | 264 | 6,96864 | 6,98559 | 0,0146074 |
| Kugel, 2/R | 0,04 | 984 | 6,96864 | 6,97763 | 0,00350241 |
| Zylinder, 1/R | 0,08 | 384 | 3,48432 | 3,48915 | 0,0186183 |
| Zylinder, 1/R | 0,04 | 1344 | 3,48432 | 3,49219 | 0,00424704 |

Beide geometrischen Maximalfehler sinken bei Halbierung der Gitterweite um mehr als Faktor 4. In einer echten nativen Kugel-MAC-PCG-Projektion betrug die Divergenz **1,09896e-15 s^-1** bei 9 PCG-Iterationen. Die gemessene verbleibende parasitaere x-Face-Geschwindigkeit **1,94287e-07 m/s** wird **nicht** als Null oder physikalisch perfektes Gleichgewicht ausgegeben. Die ebene Grenzflaeche mit Kruemmung 0 hatte 144 Faces und Divergenz **8,45435e-13 s^-1**; konstante Druckverschiebung ohne neue Stroemung wurde separat getestet.

## Negative Befunde und reproduzierte Fehler

- Erster echter Native-Lauf **FAIL** nach 5.962 Assertions: Der hergestellte ebene Test lag versehentlich zu nahe am x-Rand und wurde von der absichtlich strengen Normalen-Randpruefung abgewiesen. Die Testgeometrie wurde in die Domaenenmitte verschoben, nicht die Sicherheitspruefung abgeschaltet.
- Zweiter echter Native-Lauf **FAIL** nach 9.139 Assertions: Ein vermeintlich falscher Wet-Mask-Test setzte eine bereits nasse Zelle erneut auf nass. Der Negativtest wurde auf eine tatsaechlich widerspruechliche trockene Zelle korrigiert.
- Die endgueltige Native-Suite testet die Ablehnung negativer Oberflaechenspannung, nichtfiniter Luftdruck-/SDF-Werte, SDF-Nullwert am Zellzentrum, falscher Feldlaenge, abweichender Zellweite, inkonsistenter Nass-/Solid-Maske und fehlender Grenzflaeche. Die gescheiterten frueheren Durchlaeufe sind hier dokumentiert; die Build-Skripte ueberschreiben ihre Logdateien beim erneuten Ausfuehren.
- Die vorhandene echte **6,00-s-MARIN/SPHERIC-H1-H4-Validierung bleibt FAIL**. Es wurde **kein** neuer teurer MARIN-Dreigitter-Lauf simuliert. Keine Drucksensor-P1-P8-Messkonvergenz, keine globale FSI-/Impuls-/Energie-Abnahme und kein UE5.8-Native-E2E dieses neuen Pfads.

## Pruefbare Artefakte

`ENGINE TEST STUBE/runs/p2_sdf_capillary_{build,native,ctest}.txt` enthalten echte Build-, Native- und CTest-Ausgaben. `scripts/p2_sdf_capillary_acceptance.py` startet das native Binary unabhaengig erneut, prueft analytische Kugel-/Zylinderwerte und Gitterkonvergenz, vergleicht die Logs, verwirft absichtlich manipulierte Native-Protokolle und schreibt Quell-, Binary- und Log-SHA256 nach `ENGINE TEST STUBE/runs/p2_sdf_capillary_partial.json`. Das offizielle `scripts/phase_gate.py` bleibt massgeblich: **P2 PARTIAL, ALL_COMPLETE=False**.

**Noch offen:** dynamische VOF/SDF-Kruemmung, gekoppelte viskose Normal- und Tangentialspannungen mit Luft, volle MARIN-6-s-/Dreigitter-H1-H4/P1-P8-Messabnahme, bewegte bidirektionale Festkoerper und physikalische Gesamtbilanzen.
