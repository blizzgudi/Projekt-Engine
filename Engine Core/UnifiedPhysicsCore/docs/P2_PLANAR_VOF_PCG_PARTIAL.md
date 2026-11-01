# P2 Teilnachweis: echte schräge 3D-VOF-Planarschnitte im MAC-PCG

Stand 01.11.2026. **P2 ist nicht vollständig abgenommen.**

## Abgegrenzte technische Erweiterung

Bisher kann die optionale druckseitige 3D-VOF/PCG-Geometrie nur
strikt achsparallele planare Wasser/Luft-Grenzen auflösen.
Neue Implementierung:
`include/upc/p2_planar_vof_pcg_geometry3d.hpp`.

Sie nimmt ein VOF-Feld und **separat vorgegebene Normalen** entgegen,
bestimmt die Schnittebene je Teilzelle durch numerische Umkehrung des
tatsächlichen 3D-Schnittvolumens (64+1 Bisektionsschritte,
exakter vorhandener Cube-Clip-Operator), rekonstruiert den
Schnittpunkt mit der Verbindung der Zellmitten und übergibt die
relative physikalische Strecke theta an die bestehende
PCG-Druckgrenze.

Vorgaben: Nur benachbarte Teilzellen mit geometrisch konsistenten
Normalen und exakt übereinstimmendem Schnittpunkt gelten als planar.
Beide Wasser-/Luft-Seiten können Teilzellen sein. Entgegengesetzte,
mehrdeutige, physikalisch außerhalb der Zellverbindung gelegene
Schnitte oder NaN-Werte werden abgewiesen. Vollständig gefüllte
1/0-Nachbarzellen bleiben als geometrisch nicht aufgelöst gekennzeichnet.
Die Anwendung einer ungültigen Geometrie erfolgt transaktional,
ohne das MAC-Druckfeld teilweise zu überschreiben.

In `include/upc/coupled_water3d.hpp` ist die neue Brücke separat
und ausschließlich opt-in verfügbar:
`set_pcg_pressure_projection(true)`, anschließend
`set_planar_vof_pcg_normals(normals)` mit den **vom aufrufenden System
physikalisch begründeten** Teilzell-Normalen und schließlich
`set_planar_vof_pcg_geometry(true)`. Der existierende
achsparallele Modus bleibt unabhängig und ist mit dem neuen Modus
gegenseitig exklusiv. Ungeprüfte oder veraltete Geometrien führen
im verifizierten Live-Step zu einer atomaren Ablehnung. Sowohl
Druckprojektion vor dem Volumentransport als auch die optionale
Post-Transport-Projektion wurden nativ getestet. Keine globale
Normalenrekonstruktion und keine zweiphasige Luftspannung werden
aus der neuen Funktion behauptet.

## Unabhängige Native-/Mathematiktests

`tests/p2_planar_vof_pcg_geometry3d_tests.cpp` prüft:
- Vier analytische schräge 3D-Planarvolumina auf 8³ und 12³
  mit 45°-x/y- und x/y/z-Normalen. Referenz ist eine unabhängig
  aus der vorgegebenen Ebene abgeleitete Schnittentfernung;
  alle aufgelösten Druck-Ghost-Faces liegen innerhalb
  (2\cdot10^{-8}) Zellweiten.
- 96/144 (x/y) beziehungsweise 63/63 (x/y/z)
  aufgelöste oblique Faces, beidseitig partielles VOF und
  reale PCG-Projektion mit nasser Divergenz unter (10^{-7}\,s^{-1}).
- Die achsparallelen exakt bekannten Grenzdistanzen stimmen mit
  der bestehenden unabhängigen Achsimplementation überein.
- Ein realer `CoupledMacWater3D`-Live-Step mit bekannter
  schräger Ebene enthält 96 oblique Grenzfaces und erhält die
  Anfangswassermasse von rund 152,276 kg; ein separater
  sehr kleiner Schritt mit Reprojektion nach VOF-Transport besteht.
- Negativtests für NaN, unzulässige Nassschwelle, widersprüchliche
  Teilzell-Normalen, fehlende PCG-/Normalenfreigabe und eine
  manipulierte Face-Adresse: alle müssen fehlschlagen,
  ohne bestehenden Wasser-/Druckzustand zu verändern.

Die mathematische Rekonstruktion der Normalen aus einem VOF-Feld
ist **nicht** gelöst: Ein vorhandener zentraler Gradienten-Schätzer
hat selbst bei einem schrägen idealen Planarschnitt die exakte
Konsistenzprüfung noch nicht bestanden. Deshalb verlangt
der neue Live-Pfad explizit physikalisch überprüfte Normalen
aus einer geeigneten externen Geometriequelle, anstatt
unzuverlässige rekonstruierte Flächen als gültig auszugeben.

## Keine P2-Vollabnahme

Dieser numerische Teilnachweis validiert weder korrekt
spannungskontinuierliche bewegliche Wasser/Luft-Grenzflächen
noch vollständiges geometrisches VOF/PLIC-Transportieren.
Dynamisch deformierbare/rotierende Körper, experimentelle
3D-Druck-/Höhenkonvergenz im MARIN-Fall, vollständige FSI-
Impuls-/Energie-/Massenschlüsse und ein unabhängiger
Dreigitter-Vollnachweis fehlen.

Die bestehenden MARIN/SPHERIC-Höhen-RMSE überschreiten
weiter das vorab fixierte 0,05-m-Limit.
`phase_complete=false`, `ALL_COMPLETE=False`.
