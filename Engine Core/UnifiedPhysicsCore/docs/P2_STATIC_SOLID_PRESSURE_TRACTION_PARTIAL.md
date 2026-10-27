# P2 Teilnachweis: Druckkraft auf ein statisches 3D-Festkoerpervoxel

Stand: 27.10.2026. **P2 PARTIAL, nicht vollstaendig abgenommen.**

## Ziel und Implementierung

Die bisherige MAC-Druckprojektion `MacProjection3D` erzeugt einen
Zellmittendruck in Pa. Der neue unabhaengige Adapter
`include/upc/static_pressure_load3d.hpp` summiert auf den sechs
achsenparallelen, **benetzten** Seiten eines statischen Festkoerpervoxels
die erste Naeherung der Druckkraft auf das Solid:

`F_solid = sum_faces(p_fluid_center * n_fluid_to_solid * dx^2)` in N.

Der Adapter veraendert weder Druck noch Geschwindigkeiten, Wasseranteile
oder Festkoerperbewegung. Die explizit uebergebenen binaren Wet-/Solid-
Masken muessen zur gleichen MAC-Geometrie gehoeren. Er prueft ihre Groesse,
Ueberlappung, Rasterabstand und die Zielzelle. Die Flaechenkraefte
beruecksichtigen nur den benachbarten Fluid-Zellmittendruck; keine
Subzellen-Schnittflaechen, keine Wanddruckextrapolation, keine
Viskositaet/Schubspannung und keine Oberflaechenspannung.

**Druck-Gauge:** Bei einer geschlossenen reinen Neumann-Domaene ist
absoluter Druck bis auf eine additive Konstante undefiniert. Die hier
getestete Nettokraft eines auf allen sechs Seiten benetzten Voxels
ist davon unabhaengig, da sich konstante Gegenflaechen aufheben.
Eine teilweise benetzte oder offene Solid-Oberflaeche ist ohne
definierte Umgebungs-/Referenzdruckbedingung nicht gauge-invariant.

## Unabhaengige quantitative Referenz

Native MSVC-Release-Test: `tests/p2_static_pressure_load3d_tests.cpp`.
Ein 3D-Manufactured-Pressure-Feld mit Offset `150` bzw. `1200 Pa`,
Gradient `(7,-11,5) Pa/Zelle` und `dx=0.08 m` wird als
MAC-Face-Geschwindigkeitsgradient vorgegeben. `MacProjection3D::project`
muss die Divergenz beseitigen und das Druckfeld bis auf eine
zulaessige Gauge rekonstruieren. Eine zentrale isolierte Solid-Zelle
ist von sechs Fluidzellen umgeben.

Die **unabhaengige** Flaechenkraftreferenz folgt aus dem linearen Feld:

- `Fx = -2 * 7 * (0.08)^2 = -0.0896 N`
- `Fy = -2 * (-11) * (0.08)^2 = +0.1408 N`
- `Fz = -2 * 5 * (0.08)^2 = -0.0640 N`

Die Testmatrix umfasst `8x7x6`, `12x11x10`, `16x15x14`,
jeweils offene Wasser-Luft-Dirichlet- und geschlossene
Festkoerper-Neumann-Randbedingungen und beide Druck-Offsets
(12 echte Solver-Laeufe). Es werden sechs benetzte Flaechen,
Nettokraft, Druckrekonstruktion, Divergenz und Offset-Invarianz
geprueft. Negativtests: falsche/ueberlappende Masken, Nicht-Solid-Ziel,
ungueltige Indizes, nichtfiniter Rasterabstand und vollstaendig
trockene Festkoerperzelle.

## Reproduzierbare Abnahme

`scripts/p2_static_pressure_load_acceptance.py` validiert die
nativen CSV-Werte gegen **separat in Python berechnete** Zellanzahl
und lineare Flaechenkraft, prueft die Offset-Invarianz und verlangt
die vollstaendige MSVC-Release-CTest-Suite. Es speichert
Quell-/Binaer-/CTest-SHA256 unter:

`ENGINE TEST STUBE/runs/p2_static_pressure_load_partial.json`

CTest-Log:
`ENGINE TEST STUBE/runs/p2_static_pressure_load_ctest_20261027.txt`

Ein gruener Teilnachweis setzt `partial_accepted=true`,
`phase_complete=false` und gueltige Hashes voraus. Der
konservative `scripts/phase_gate.py` bleibt fuer P2–P8 **PARTIAL**.

## Grenzen / naechster Schritt

Diese Drucklast ist ein **statischer Druck-Flaechenintegral-Proxy**,
kein bidirektionaler Fluid-Solid-Impulsbeweis. Der naechste Schritt
muss eine dynamische, zeitaufgeloeste Solid-Grenze mit kontrolliertem
Volumen-/Impulsaustausch und unabhaengiger Impuls-/Energiebilanz
implementieren und gegen echte 3D-Referenzdaten testen. Auch
spannungsrichtige freie Oberflaechen, thermische/reaktive
Mehrmaterialkopplung, GPU/LOD-Integration und UE5.8-Native-E2E
sind dadurch **nicht** abgenommen.
