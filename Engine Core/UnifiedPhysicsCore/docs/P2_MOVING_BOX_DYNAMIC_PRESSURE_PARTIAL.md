# P2 Teilabnahme: dynamische Quadertranslation aus eingefrorenem 3D-Druck

Stand: 29.10.2026. **P2 PARTIAL, P3–P8 PARTIAL, ALL_COMPLETE=False.**

## Implementierter Umfang

`P2MovingBoxDynamicPressure3D::step` verbindet erstmals die reale
Cut-Cell-Drucktraktion aus `P2MovingBoxPressureTraction3D` mit
`RigidPressureResponse3D`. Ein zellzentriertes Druckfeld wird auf
der **alten** Boxoberfläche ausgewertet und für einen expliziten
Zeitschritt als konstant angenommen. Der Impuls `F*dt` wird auf
die lineare Geschwindigkeit des dynamischen Festkörpers angewendet.
Die trapezförmige Verschiebung aktualisiert seinen Schwerpunkt und
die achsenparallele Box. Der neue Cut-Cell-Sweep wird geometrisch
geprüft (Volumen, Domain und Zell-CFL), bevor ein Ergebnis zurückgeht.

Die Funktion ist transaktional: Eingabedruck, Ausgangsbox und
Ausgangszustand bleiben auch bei Ablehnung unverändert. Nichtnull-
Drehmoment und Anfangsrotation werden bewusst abgelehnt, da eine
gedrehte Box ohne Orientierungs- und Kollisionsgeometrie falsch wäre.
Ein Feld mit sprunghafter Traktion darf zwischen alter und
mittlerer Position eine andere Kraft liefern: diese Abweichung
wird ausgegeben und **nicht** als physikalische Erhaltung kaschiert.

## Reale Nachweise

- MSVC Release und vollständige **78/78 CTest-Suites PASS**.
- 12 unabhängige zweischrittige analytische 3D-Fälle:
  8³/12³, 3 Achsen, beide Druckgradient-Vorzeichen.
  Box [3.2,4.8]³ m, `dp/dx = +/-10 Pa/m`,
  `m=10 kg`, `dt=0.1 s`. Oberflächenfläche 2.56 m²,
  Kraft `F = -25.6*sign N`, nach zwei Schritten
  `v=-0.512*sign m/s`, `x=4-0.0512*sign m`,
  `delta_K=1.31072 J` und kumulativer Gegenimpuls
  `J_fluid=+5.12*sign N*s` **nur als Ledger**.
- Unabhängige analytische Prüfung von
  `delta_K=F*delta_x`, diskreter geometrischer Druckarbeit,
  Null-Drehmoment, Druck-Gauge-Invarianz und ballistischer Bewegung.
- Negativfälle: falscher Schwerpunkt, Rotation, Nullmasse,
  Nullzeitschritt, falsche Druckgröße, NaN, Zell-CFL,
  Domain-Überschreitung, nichtnull-Drehmoment.
- `scripts/p2_moving_box_dynamic_pressure_full_build.cmd` und
  `scripts/p2_moving_box_dynamic_pressure_acceptance.py`.
  Belege: `ENGINE TEST STUBE/runs/p2_moving_box_dynamic_pressure_*`.

## Grenzen und nächste Arbeit

**Dies ist noch keine bidirektionale FSI.** Der Druckimpuls wirkt
tatsächlich auf die dynamische Box, aber der entgegengesetzte
Fluidimpuls wird **nicht** auf MAC-Faces/VOF oder Fluiddruck angewendet.
Die summierte Null im Impuls-Ledger ist nur eine Identität, keine
globale Impulserhaltung des simulierten Fluids. Die Gleichheit von
kinetischer Energie und eingefrorener Druckarbeit ist die exakte
Konstantkraft-Identität; es fehlt eine dynamisch gekoppelte
Fluid-Festkörper-Energiebilanz. Kein Rotations-/Kontaktintegrator,
kein neuer UE5.8-Native-E2E, keine freie Wasseroberflächen-Spannung
und keine unabhängigen gemessenen 3D-CFD-Referenzen.

Nächster Schritt: Impuls mit konsistenten Cut-Cell-MAC-Fluidflüssen
tatsächlich übertragen, Geometrie und Druckprojektion iterieren,
danach Gesamtimpuls-/Energie- und VOF-Massenbilanz testen.
