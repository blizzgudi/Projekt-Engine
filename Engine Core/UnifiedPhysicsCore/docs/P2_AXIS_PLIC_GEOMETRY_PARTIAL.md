# P2 Teilabnahme: axis-aligned VOF-PLIC-Geometrie (31.10.2026)

## Implementierter Umfang

Die optionale, schreibgeschützte Funktion p2_axis_plic_geometry3d in
include/upc/p2_axis_plic_geometry3d.hpp berechnet Masse, Schwerpunkt,
Trägheitstensor um Ursprung und Schwerpunkt für echte achsparallele
planare Flüssigkeitsschnitte in 3D-Voxeln. Bei einer Teilfüllung f
ist die Wasserregion ein Quader mit einer Kantenlänge f*dx; Achse
(0,1,2) und Vorzeichen (+1: von der unteren, -1: von der oberen
Zellfläche) werden ausdrücklich als Eingabe vorgegeben. Leere und
volle Zellen sind richtungsunabhängig. Überlappungen mit Festkörpern,
nichtfinite Werte, ungültige Achsen/Orientierungen und falsche
Arraygrößen werden abgelehnt. Keine Veränderung des Solvers.

Für die Teilzelle ist die intrinsische Schwerpunktträgheit exakt
I_ii=m*(w_j²+w_k²)/12. Die bisherige uniforme Teilzellennäherung
m*dx²/6 auf allen Diagonalen ist bei partieller Füllung allgemein
nicht korrekt. Beispiel: 0,2-m-Würfel, 50 % Wasser entlang x,
rho=1000 kg/m³: Masse 4 kg, Schwerpunkt x=0,05 m,
Ixx=0,0266666667, Iyy=Izz=0,0166666667 kg m².
Bei umgekehrtem Schnitt liegt der Schwerpunkt x=0,15 m.

## Nachweise und Grenzen

- Analytische Halbzelle und Schnittorientierung entlang aller drei
  Achsen, positive und negative Schnittseite.
- Voller 0,4-m-Würfel auf 8³ und 12³ Zellen: 64 kg und
  I_COM=1,7066666667 kg m².
- Asymmetrische partielle 8³-/12³-Füllungen mit unterschiedlichen
  Achsen, Vorzeichen und gesperrten Festkörperzellen: unabhängige
  2×2×2-Gauß-Legendre-Volumenquadratur für sämtliche 9
  Trägheitstensorkomponenten, Schwerpunkt, Masse und
  Ursprungsverschiebung.
- Reproduktion: scripts/p2_axis_plic_full_build.cmd und
  scripts/p2_axis_plic_acceptance.py. Native MSVC-Release-Build,
  **98/98 CTest-Suites**, **125 native Assertions** und **33/33**
  quantitative Teil-Gates PASS. P1 33/33 und P2-MVP 30/30 erneut PASS.
  SHA256-Fingerprints und JSON-Bericht unter
  ENGINE TEST STUBE/runs/p2_axis_plic_*.

**P2 bleibt PARTIAL.** Dies ist ein exakt begrenztes Geometriemodell
für achsparallele Ebenenschnitte, **kein** beliebig orientiertes
PLIC-Interface, kein Normalen-Rekonstruktionsverfahren, kein
Massen-/Impuls-/Energietransport und keine bewegte Zweiwege-FSI.
Die geometrischen Momente werden noch nicht im MAC/VOF-Live-Solver
verwendet. Unabhängige 3D-CFD-Messdaten und ein neues UE5.8-Native-E2E
fehlen. P3-P8 bleiben ebenfalls offen.
