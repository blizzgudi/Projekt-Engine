# P2-Teilnachweis: stratified VOF -> hydrostatic pressure -> rigid body

**Status: PARTIAL, nicht P2-Vollabnahme.** Der optionale
`P2StratifiedHydrostaticLoad3D`-Pfad erzeugt aus vertikal
stratifizierten Wasserfüllfraktionen einen hydrostatischen Druck
`p=rho*g*(h-z_center)` in vollständig nassen Zellen und
`p=rho*g*f*dx/2` in der teilweise nassen obersten Zelle
als **wasservolumengemitteltes** diagnostisches Feld. Die
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

**Erweiterte Teilzellenvalidierung:** Die analytische Druckmittelung
in einer teilweise gefüllten Zelle verwendet nur das tatsächlich
benetzte Volumen. Eine unabhängige Integration der gesamten vertikalen
Wassersäule prüft `sum_z(p_z*f_z*dx)=rho*g*h²/2` für jede Spalte.
Das ist eine hydrostatische Diagnose, keine freie-Oberflächen-CFD
und keine auf MAC angewendete Fluid-Gegenreaktion.

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

**Historischer Ausgangsnachweis (29.10.2026):** 82/82 CTest PASS,
13.669 native Assertions, 53 quantitative Gate-Checks, 12
3D-Referenzfaelle (8³/12³). Bei rho=1000 kg/m³, g=9,81 m/s²,
A=2,56 m² und Quader z=3,2..4,8 m: trocken 0 N,
Wasserlinie z=4,0 m 20.090,88 N, voll eingetaucht 40.181,76 N.
Der diskrete zellzentrische Druckunterschied alleine waere fuer
den voll eingetauchten 1,6-m-Quader falsch; die direkte
Oberflaechenintegration verhindert diesen Fehler.

**Teilzellen-Erweiterung (29.10.2026):** MSVC Release und volle
82/82 CTest-Suites PASS; 29.821 native Assertions, 24 analytische
3D-Faelle (8³/12³; trockene, ganzzahlige und fraktionale Wasserlinien,
Schwerkraft 0/9,81 m/s²). Die unabhaengige exakte vertikale
Wasser-Druckintegration wird in jeder Spalte geprüft. Die
maschinelle Teilabnahme umfasst **101 quantitative Checks** und prueft
zusätzlich die Quaderlasten, Impulse, Quellhashes und den vollstaendigen
CTest-Log. P2 bleibt PARTIAL.
