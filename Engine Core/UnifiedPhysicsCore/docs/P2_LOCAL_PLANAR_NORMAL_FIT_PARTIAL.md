# P2 partial: lokal volumenangepasste PLIC-Normalen fuer schraege 3D-VOF-Grenzen

Stand 2026-11-01. **P2 INCOMPLETE.** Keine experimentelle oder globale physikalische Vollabnahme.

## Reproduzierbare Erweiterung

`include/upc/p2_local_planar_normal_fit3d.hpp` fuegt eine opt-in, rein geometrische
Normalenrekonstruktion fuer **lokal planare** VOF-Grenzflaechen hinzu. Anders als
der bestehende zentrale Fuellgradgradient werden zwei freie Orientierungsparameter
gegen die exakten Nachbar-Zellvolumenanteile optimiert. Das Volumen der zentralen
Teilzelle wird bei jeder Kandidatennormale durch die vorhandene Cube-Clip-
Bisection exakt angepasst. Ein lokaler 3x3x3-Patch, der nicht innerhalb der
expliziten Fuellgrad-Toleranz durch eine Ebene beschrieben werden kann, wird
abgelehnt. Solids werden aus dem Fit ausgeschlossen. Die Eingabedaten bleiben
unveraendert. Kein automatischer Wechsel des produktiven Live-Solvers.

Die Funktion liefert Normalen, Residuen und Iterationszaehler; die bestehende
`p2_planar_vof_pcg_geometry3d` kann damit die PCG-Grenzdistanzen
ermitteln. **Weder** dynamische Grenzflaechenspannung noch zweiphasiger
viskoser Traktionsausgleich, Oberflaechenspannung, Kontaktwinkel oder eine
bewegte Festkoerperkopplung werden hier implementiert.

## Echte native MSVC-/Mathematik-Nachweise

`scripts/p2_local_planar_normal_fit_build.cmd` baut unter Windows mit
MSVC Release die gesamte CMake-Konfiguration und fuehrt die vollstaendige
CTest-Suite aus. `tests/p2_local_planar_normal_fit3d_tests.cpp` verwendet
synthetische, **unabhaengig analytisch definierte** x/y- und x/y/z-
Ebenen mit zwei bzw. drei Normalenkomponenten sowie eine schraege
Ebene mit negativem z-Anteil. Jede Zellfraktion stammt aus dem
exakten Cube-Clip der vorgegebenen Ebene. Nach dem automatischen
Normalenfit wird die Geometrie mit dem analytischen Schnittabstand
verglichen und die echte MAC-PCG-Projektion auf Nasszell-Divergenz
geprueft. 8^3 und 12^3 sind separate Raster.

| Raster | Ebenen | Teilzellen | Druck-Grenzfaces | Schlechtester lokaler Fuellgradfehler |
| --- | ---: | --- | --- | ---: |
| 8^3 | 3 | 104 / 85 / 127 | 96 / 63 / 119 | 1.879e-10 |
| 12^3 | 3 | 228 / 166 / 287 | 216 / 165 / 275 | 1.879e-10 |

Die gemessene minimale Normalenausrichtung aller sechs Referenzfaelle
liegt numerisch bei 1.0 (innerhalb Maschinenpraezision).
**1977 native Assertions PASS; 125/125 CTest PASS.** Der negative
Test mit absichtlich veraendertem Teilzellvolumen wird verworfen;
ebenso ungueltige Toleranzen und NaN-Eingaben. Der unabhaengige
Python-Parser verwirft sechs manipulierte Native-Protokolle und
wiederholt das echte MSVC-Testbinary mit identischer Ausgabe.

Artefakte mit SHA256:
- `ENGINE TEST STUBE/runs/p2_local_planar_normal_fit_build.txt`
- `ENGINE TEST STUBE/runs/p2_local_planar_normal_fit_ctest.txt`
- `ENGINE TEST STUBE/runs/p2_local_planar_normal_fit_native.txt`
- `ENGINE TEST STUBE/runs/p2_local_planar_normal_fit_partial.json`

Reproduzieren: erst das Build-CMD, dann
`python "Engine Core/UnifiedPhysicsCore/scripts/p2_local_planar_normal_fit_acceptance.py"`
aus `D:\Projekt Engine`.

## Negativbefunde und Grenzen

Die synthetischen Ebenen sind keine experimentellen SPHERIC/MARIN-Daten.
Der bisherige echte 6-s-MARIN-Vergleich hatte H1-H4-RMSE
0.136525/0.091500/0.148245/0.224399 m; alle verletzen 0.05 m.
Keine neue Messkonvergenz, kein feines/drittes 6-s-Raster und kein
nachgewiesener physikalischer Wanddruck-PASS. Bei gekruemmten,
topologisch wechselnden oder numerisch gestoerten Grenzflaechen
kann der strenge lokale Planar-Fit scheitern; das ist eine
**bewusste Ablehnung**, keine validierte universelle Rekonstruktion.
Der neue Operator ist nicht in die Live-VOF-Advektion integriert.

Es fehlen weiterhin stresskonsistente Wasser/Luft-Grenzbedingungen,
dynamische Zweiwege-FSI, globale Massen-/Impuls-/Energiebilanzen,
H1-H4/P1-P8-Vollfenster-/Dreigitter-Messkonvergenz und UE5.8-Native-E2E
dieses Pfades. `phase_complete=false`, P2 **PARTIAL**.
