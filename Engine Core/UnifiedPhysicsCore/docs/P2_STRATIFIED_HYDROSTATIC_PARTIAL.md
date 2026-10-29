# P2-Teilnachweis: stratified VOF -> hydrostatic pressure -> rigid body

**Status: PARTIAL, nicht P2-Vollabnahme.** Der optionale
`P2StratifiedHydrostaticLoad3D`-Pfad erzeugt aus vertikal
stratifizierten Wasserfüllfraktionen einen hydrostatischen Druck
`p=rho*g*max(h-z_center,0)` als diagnostisches Feld. Die
tatsächliche hydrostatische Druckkraft auf den starren Quader wird
**an den realen Oberflächen** analytisch integriert, nicht aus
zellzentrischen Druckwerten approximiert:
`F_z=rho*g*A*clamp(h-z_min,0,z_max-z_min)` (Archimedes).
Damit beeinflusst der Wasserstand die Quadertranslation direkt.

Die unabhängige analytische Referenz prüft die verdrängte Wassermenge,
Kraft, Impuls, Geschwindigkeit, Mittelpunkt- und Boxverschiebung,
symmetrisch verschwindende Drehmomente, mechanische
Körper-Arbeitsidentität und unveränderte Eingaben. Getestet werden
8³/12³, leere/teilgefüllte/vollgefüllte Becken und g=0/9,81 m/s².
Ungültige oder nicht horizontale Wasserspiegel werden abgewiesen.

**Grenzen:** Nur ruhende, horizontal einheitliche Wasserstände mit
unten gefüllten vertikalen Wassersäulen. Keine freien Wellen, keine
Wasserströmung und keine externe CFD-Messreferenz. Die hydrostatische
Last bewegt den Festkörper, aber weder der Gegenimpuls noch der
Wasserstand werden hier in den Fluid-MAC-Solver zurückgeführt.
Eigengewicht und äußere Auflager des Körpers sind nicht enthalten.
Die Körper-Arbeitsidentität beweist keine Fluid-Festkörper-Energiebilanz.
Keine vollständige Fluid-Festkörper-Impulserhaltung oder
UE5.8-Native-E2E dieses Pfades.

Belege: `tests/p2_stratified_hydrostatic_load3d_tests.cpp`,
`scripts/p2_stratified_hydrostatic_acceptance.py`,
`ENGINE TEST STUBE/runs/p2_stratified_hydrostatic_partial.json`
und das vollständige CTest-Protokoll.

**Messwerte (MSVC Release, 29.10.2026):** 82/82 CTest PASS,
13.669 native Assertions, 53 quantitative Gate-Checks, 12
3D-Referenzfaelle (8³/12³). Bei rho=1000 kg/m³, g=9,81 m/s²,
A=2,56 m² und Quader z=3,2..4,8 m: trocken 0 N,
Wasserlinie z=4,0 m 20.090,88 N, voll eingetaucht 40.181,76 N.
Der diskrete zellzentrische Druckunterschied alleine waere fuer
den voll eingetauchten 1,6-m-Quader falsch; die direkte
Oberflaechenintegration verhindert diesen Fehler.
