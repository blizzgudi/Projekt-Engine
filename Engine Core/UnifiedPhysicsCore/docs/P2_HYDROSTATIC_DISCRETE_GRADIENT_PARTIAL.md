# P2 Teilabnahme: hydrostatische diskrete Gradientenintegration (29.10.2026)

**Status: PARTIAL, keine P2-Vollabnahme.** Optionaler, rein hydrostatischer
Starrkoerper-Zeitschritt in einer vorgeschriebenen ruhenden, horizontalen
Wasseroberflaeche. Er ersetzt weder den MAC/VOF-Solver noch die FSI-Kopplung.

## Technischer Unterschied

`P2HydrostaticGravity3D::step` verwendet die anfängliche Auftriebskraft
ueber den gesamten Zeitschritt. Bei geaenderter Eintauchtiefe entsteht ein
messbarer Fehler zwischen kinetischer Energie und exaktem hydrostatischem
Potential. Der neue optionale
`P2HydrostaticDiscreteGradient3D::step` verwendet stattdessen die
**exakt entlang des Koerperwegs gemittelte Auftriebskraft**.
Die stueckweise lineare benetzte Hoehe wird an den zwei Nass/Trocken-
Grenzen zerlegt und analytisch integriert. Damit wird bei sehr kleinen
Verschiebungen keine Differenz nahezu gleicher grosser Potentialwerte
gebildet.

Fuer die vertikale Bewegung gelten

- `dz = dt*(v0+v1)/2`
- `m*(v1-v0) = dt*(F_b,avg-m*g)`
- `F_b,avg = rho*g*A*mean_z(clamp(h-z+H/2,0,H))`
- `delta K + m*g*dz - F_b,avg*dz = 0` (bis Rundung)

Die skalare implizite Gleichung wird mit monotoner Intervallhalbierung
(100 Schritte, gepruefte Klammer) geloest. Die alte Integrationsmethode
bleibt unveraendert und wird als expliziter Vergleich mit ausgegeben.

## Native Tests und Nachweise

`tests/p2_hydrostatic_discrete_gradient3d_tests.cpp` prueft 48
Kombinationen (8³/12³, sechs Wasserstaende, g=0/9.81 und zwei
Anfangsgeschwindigkeiten), unabhaengige numerische Wegmittelung
(4096 Integrationsintervalle), Impuls-/Energiebilanzen, erhaltene
Eingabedaten, Gleichgewicht, zeitumgekehrten Schritt, zwei echte
Nass/Trocken-Regimewechsel und negative Eingaben.

Der vollstaendige MSVC-Release-Build mit 84/84 CTest-Suites, die
native Assertion-Zahl, quantitative Python-Checks und SHA256
werden in `ENGINE TEST STUBE/runs/p2_hydrostatic_discrete_gradient_partial.json`
und `p2_hydrostatic_discrete_gradient_full_ctest_20261029.txt`
belegt. Nach jedem Quellcodeaendern sind Build, Tests und Gate neu
auszufuehren.

## Grenzen

Dies ist **nur eine mechanisch energieerhaltende Integration des
Starrkoerpers in einem festgehaltenen Wasserpotential**. Die physikalische
Energie des Wassers wird nicht entwickelt; der entgegengesetzte
Auftriebsimpuls wird nur ausgewiesen und nicht auf MAC-Faces angewendet.
Keine dynamische Wasseroberflaeche, keine Scher-/Druckspannungs-FSI,
kein globaler Impuls-/Energiebeweis, keine unabhängigen 3D-CFD-Messdaten,
kein Unreal-5.8-Native-E2E fuer diesen Pfad. Der konservative Integrator
uebernimmt die strengeren Geometrie-/CFL-Vorpruefungen des bestehenden
eingefrorenen Referenzschritts.
