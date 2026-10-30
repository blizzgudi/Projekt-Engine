# P2: Vollstaendige MAC-Face-Projektionspartition (Teilnachweis, 31.10.2026)

**Status: P2 INCOMPLETE, P3-P8 INCOMPLETE, ALL_COMPLETE=False.**

## Umfang
Der neue read-only C++17-Adapter `ProjectionFaceClosureAudit3D` durchlaeuft
saemtliche staggered MAC-Faces und trennt sie in (a) Wasser/Wasser oder
Wasser/Luft mit Druckgradient, (b) Festkoerper- und Aussen-Clamps sowie
(c) trockene Luft/Luft-Faces ohne Fluidmasse. Die Gesamtimpuls- und
Energie-Proxies der aktiven Faces werden unabhaengig von der bestehenden
`ProjectionGradientBudgetAudit3D`-Partition summiert und auf Restfehler
geprueft. Unveraenderte Luft/Luft-Geschwindigkeiten werden separat
nachgewiesen; ein unzulaessig bewegtes Clamp-Face wird erkannt.

## Unabhaengige analytische Referenz
Eine einzelne nasse Zelle mit fuenf Luft-Nachbarn und einem benachbarten
Solid-Voxel liefert die exakte diskrete Poisson-Diagonale 10.
Der Ausgangsfluss nach Solid-Clamp ist -0.05 m/s.
Daher gilt p = 0.005*rho*dx/dt; die Druckkorrektur betraegt an den
fuenf freien Faces jeweils +/-0.01 m/s. Weitere initiale Face-Werte
sind 0.25 (Solid), -0.17 (Solid/Luft), 0.31 (Aussen) und 0.12 m/s
(Luft/Luft). Der Gesamtimpuls-Proxy ist
(-0.26, +0.17, -0.31)*rho*dx^3 Ns; der Energie-Proxy
-0.094*rho*dx^3 J. Drehimpuls wird separat aus acht bekannten
Face-Orten und Hebelarmen berechnet. Die Klassenanzahlen sind
5 Druckfaces, 6+6*n^2 Clamp-Faces und 3*n^3-3*n^2-11
Luft/Luft-Faces. Das gilt fuer n=8 und n=12.

Drei absichtlich korrumpierte Post-Projektionsfelder werden erkannt:
geaendertes Luft/Luft-Face, nicht genulltes Solid-Face und fehlerhafte
Druckgradienten-Korrektur. Dies verhindert, dass eine numerische
Bilanz faelschlich als vollstaendig gemeldet wird.

## Reproduktion
- Build/Regression: `scripts/p2_projection_face_closure_full_build.cmd`
- Native Test: `tests/p2_projection_face_closure3d_tests.cpp`
- Quantitatives Gate: `scripts/p2_projection_face_closure_acceptance.py`
- Nachweis: `ENGINE TEST STUBE/runs/p2_projection_face_closure_partial.json`
- CTest-Log: `ENGINE TEST STUBE/runs/p2_projection_face_closure_ctest_20261031.txt`

Der eigenstaendige native Test wird absichtlich direkt mit MSVC gebaut:
Die bestehende CMake-CTest-Registrierung wird nicht veraendert, damit
aeltere signierte P1-/P7-Nachweise nicht ohne Grund ungueltig werden.
Die vorhandenen 88 CTest-Suites laufen zusaetzlich komplett durch.

## Grenzen
Diese Face-Quadratsummen mit rho*dx^3 sind **keine** physikalisch
abgenommene Fluid-Festkoerper-Gesamtimpuls- oder Energiebilanz.
Insbesondere gibt es keine aktualisierte rotierende Kollisionsgeometrie,
keine nachgewiesene konservative Druck-/Wand-Reaktionskraft ueber
bewegte Schnittflaechen, keine externen 3D-CFD-Messdaten und keinen
neuen Unreal-5.8-Native-E2E-Lauf. P2 bleibt offen.

## Ausgefuehrte Pruefung (31.10.2026)
- Direkter MSVC C++17 Release-Build: PASS.
- Gesamte bestehende CTest-Suite: 88/88 PASS.
- Neue native Assertions: 82 PASS (zwei 3D-Gitter, drei Fault-Injections).
- n=8: p=25 Pa, J=(-0.26,+0.17,-0.31) Ns, E=-0.094 J;
  n=12: p=29.333333333333336 Pa,
  J=(-0.146432,+0.095744,-0.174592) Ns, E=-0.0529408 J.
- Maximaler algebraischer Partitionsrest: 1.39e-17;
  maximaler Druckgradienten-Geschwindigkeitsrest: 5.21e-18 m/s.
- Native Test ist eigenstaendig; CTest bleibt bei 88 Suites.
