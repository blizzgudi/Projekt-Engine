# P2 Teilabnahme — exakt integrierter 3D-Wand-Sweep / lokale GCL

Stand: 2026-10-28. **P2 bleibt PARTIAL.** Dieser Nachweis erweitert
`P2MovingBoxFaceApertures3D` und `P2MovingBoxOpenVolumes3D`, ohne
bestehende Solverpfade zu ändern.

## Was implementiert wurde

`include/upc/p2_moving_box_swept_wall_flux3d.hpp` erzeugt für einen
achsenparallelen, starr und linear verschobenen Quader aus zeitgemittelten
MAC-Face-Blockierungsanteilen die **vorzeichenbehafteten geometrischen
Festkörper-Sweep-Volumina** je Face:

`Q_solid,axis = (box_new.min[axis] - box_old.min[axis]) * dx^2 * (1 - face_open_average)`.

Die Einheit ist m³ pro Zeitschritt, **nicht** m³/s und **nicht**
Flüssigkeits-MAC-Geschwindigkeit. Bei konstantem Wandtempo entspricht
dies exakt dem zeitlichen Integral der vom Festkörper durchsetzten
Face-Fläche multipliziert mit der Normalgeschwindigkeit.

Für jede Zelle wird die diskrete geometrische Erhaltungsgleichung
`(alpha_new - alpha_old)*dx^3 = sum(outward signed Q_solid)`
mit lokalem Residuum kontrolliert. Auch die äußere Domänenbilanz und
das Gesamtvolumen werden geprüft. Ungültige Geometrie, Geometrie-CFL,
nichtfinite Zahlen und GCL-Fehler führen zu einer Exception; Eingaben
bleiben unverändert. Der Operator liefert eine neue Struktur und
verändert den bisherigen Live-Wassersolver nicht.

## Unabhängige analytische Referenzen

- Reine positive und negative Achsverschiebung um 1 m bei
  `[2.25,3.25]` m in Bewegungsrichtung: Face-Sweeps
  `sign*0.75 m³` und `sign*0.25 m³`.
- Gekoppelte Bewegung `(+1,+0.5,-0.25)` m in permutierten Achsen:
  exaktes Polynom `integral_0^0.75 (0.75-0.5t)(0.75+0.25t) dt
  = 45/128 m³` am betrachteten Normal-Face.
- 8³, 12³ und 16³ Gitter, alle drei Achsen, beide Vorzeichen,
  Zeitumkehr (Flussvorzeichen), alternative Zeitschritte,
  stationäre Box, 18 deterministische subzellgenaue diagonale
  Verschiebungen je Gitter und ungültige Eingaben.
- Numerische Abnahme liest echte native Executable-Ausgabe, prüft
  unabhängige Sollwerte, vollständige CTest-Regression und
  SHA-256 der maßgeblichen Quellen.

Nachweise unter `D:\Projekt Engine\ENGINE TEST STUBE\runs`:
`p2_swept_wall_full_ctest_20261028.txt`,
`p2_moving_box_swept_wall_partial.json`.
Erzeugung: `scripts/p2_moving_box_swept_wall_acceptance.py`.

## Was weiterhin fehlt

Diese geometrische GCL-Quelle wird **noch nicht** mit der
fraktionalen VOF-Flüssigkeitskontinuität, Cut-Cell-Druckprojektion,
Wandspannungen oder einem bidirektionalen Starrkörperimpuls gekoppelt.
Sie garantiert weder positive Flüssigkeitsfüllung noch einen
physikalisch zulässigen Druckfluss. Nur reine Translation ohne Rotation,
Deformation, komplexe Geometrie oder extern gemessene 3D-CFD-Referenz.
Eine neue UE5.8-Native-E2E-Integration wurde hierfür nicht getestet.
Keine vollständige P2-, P3- oder P8-Abnahme.
