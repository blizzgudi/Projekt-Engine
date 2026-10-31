# P2 Teilabnahme: beliebig orientierte planare VOF-Schnitte (31.10.2026)

## Umfang

`p2_oblique_plic_geometry3d` in `include/upc/p2_oblique_plic_geometry3d.hpp`
berechnet bei vorgegebenem Normalenvektor je teilgefüllter Zelle die
exakten geometrischen Momente einer ebenen, homogen mit Wasser gefüllten
Würfel-Teilmenge. Die Ebene wird per Volumen-Bisektion so platziert, dass
der vorgegebene VOF-Füllgrad getroffen wird. Die sechs Würfelflächen
werden gegen die Halbebene beschnitten; die neue polygonale Schnittfläche
wird orientiert geschlossen. Gerichtete Dreieckstetraeder liefern Volumen,
erste und zweite Momente. Daraus folgen Masse, Schwerpunkt und voller
Trägheitstensor einschließlich der Nebendiagonalen. Der Aufruf ist read-only.
Normale und Füllgrad sind Eingaben, **keine** aus Nachbarzellen rekonstruierte
PLIC-Normale. Für leere/volle Zellen sind Nullnormalen zulässig.

## Unabhängige analytische Referenzen

- Ebene x+y+z <= 1 in der Einheitszelle: Tetraedervolumen 1/6,
  Schwerpunkt (1/4,1/4,1/4), Schwerpunktkovarianz diagonal 3/80,
  Nebendiagonalen -1/80. Bei dx=0,2 m und rho=1000 kg/m³:
  4/3 kg, Schwerpunkt (0,05 m)^3, Ixx=0,004 kg m² und
  Ixy=0,0006666666667 kg m².
- Ebene x+y <= 1: dreieckiges Prisma mit Volumen 1/2,
  Schwerpunkt (1/3,1/3,1/2), Kovarianzen var(x)=var(y)=1/18,
  var(z)=1/12, cov(x,y)=-1/36. Bei dx=0,2 m:
  4 kg, Ixx=0,0222222222222 kg m², Ixy=0,0044444444444 kg m².
- Sechs achsparallele Schnitte mit Normalen +/-x, +/-y, +/-z
  werden gegen den vorherigen unabhängigen Achsenadapter geprüft.
- Zwei asymmetrische 8³-/12³-Mischfelder: VOF-Massenreferenz direkt
  aus den Füllgraden, Tensor-Symmetrie, positive Schwerpunktträgheit,
  Ursprungstransformation und unbenetzte Solid-Voxel.
- Ungültige Füllgrade, NaN/Inf, Überlappung mit Festkörper,
  fehlende Daten und Nullnormale bei Teilfüllung werden zurückgewiesen.

## Nachweis und Grenzen

Reproduzierbar mit `scripts/p2_oblique_plic_full_build.cmd` und
`scripts/p2_oblique_plic_acceptance.py`; die zugehörigen MSVC-/CTest-
Protokolle und JSON/SHA256-Belege liegen unter
`ENGINE TEST STUBE/runs/p2_oblique_plic_*`.
MSVC-Release-Build und vollständige **99/99 CTest-Suites** bestanden;
**175 native Assertions** und **26/26** unabhängige quantitative
Teilprüfungen PASS. P1-Basisabnahme **33/33**, P2-MVP **30/30**
erneut erfolgreich. Alle Fingerprints sind im JSON-Bericht enthalten.

Die Berechnung arbeitet auf einem mathematisch idealen Einzelplanarschnitt
pro Zelle. Sie validiert **nicht** PLIC-Normalenrekonstruktion,
krümmungsabhängige Oberflächenspannung, konservative Grenzflächentransporte,
bewegte Fluid-Festkörper-Geometrie oder globale Impuls-/Energieerhaltung.
Analytische Geometriereferenzen sind **keine** unabhängigen CFD-Messdaten.
Unreal-5.8-Native-E2E wurde für diesen Pfad nicht neu ausgeführt.
P2–P8 bleiben INCOMPLETE; P0/P1 behalten ihren separat nachzuweisenden
Basisstatus.
