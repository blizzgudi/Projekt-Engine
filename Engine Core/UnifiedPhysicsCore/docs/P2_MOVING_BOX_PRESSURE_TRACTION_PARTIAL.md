# P2 Teilabnahme: Drucktraktion und Gegenimpuls-Diagnose am bewegten Quader

Stand: 29.10.2026. **P2 PARTIAL, P3–P8 PARTIAL, ALL_COMPLETE=False.**

## Umfang

Die neue optionale, nicht mutierende C++-Komponente
`P2MovingBoxPressureTraction3D::audit` integriert den zellzentrierten
Druck über die sechs Oberflächen eines starr übersetzten Quaders.
Für jede Oberflächenkachel wird die geometrisch exakte Schnittfläche
mit den tangentialen Voxelzellen bestimmt. Der Druck wird stückweise
konstant aus der jeweils **außerhalb** der Oberfläche liegenden
Cut-Cell übernommen. Die Oberfläche liegt zeitlich am Mittelpunkt
zwischen alter und neuer Quaderposition.

Ausgegeben werden die resultierende Druckkraft auf den Festkörper
`F_solid = -sum(p*n_solid*dA)`, das Drehmoment um den Quadermittelpunkt,
ein **nur diagnostischer** gleich großer Gegenimpuls für das Fluid
`J_fluid = -F_solid*dt` und die mechanische Arbeit des
Mittelpunkts-Ansatzes `F_solid·(x_new-x_old)`.

Unabhängig davon wird die **diskrete geometrische Druckarbeit**
`W_fluid = sum(p_i*(alpha_old_i-alpha_new_i)*dx^3)`
aus den exakt geschnittenen Voxelvolumina berechnet.
`W_solid = -W_fluid`. Der Unterschied zwischen
`F_solid·delta_x` und `W_solid` wird **explizit gemeldet**, nicht
verschwiegen oder als allgemein null vorausgesetzt. Beide können bei
stückweise konstantem Druck und beliebiger Geometrie unterschiedlich
sein. Diese Drucktraktion enthält **keine** viskose Schubspannung.

## Tatsächliche Prüfung

- MSVC-Release-Build und **77/77 vollständige CTest-Suites PASS**.
- Native Testdatei `tests/p2_moving_box_pressure_traction3d_tests.cpp`:
  8³ und 12³, drei Bewegungsachsen und zwei Vorzeichen (12 Fälle),
  unabhängige analytische 40-N-Druckkraft, 10-kg-m/s-Gegenimpuls,
  ±8-J-Swept-Work, 24-m²-Oberfläche, Null-Drehmoment,
  Druck-Gauge-Invarianz und Nichtmutation.
- Ein asymmetrischer Druckpatch testet unabhängig 10 N,
  -5/+5 Nm und 2 J. Zusätzlich echte Druckfelder aus dem
  bewegten Cut-Cell-PCG-Solver, Gauge-Test und Negativfälle
  für Dimension, NaN, Zeitschritt und Geometrie-CFL.
- Das Python-Teilgate kontrolliert die vollständige CTest-Ausgabe,
  die nativen Referenzzeilen, analytische Werte, Quell-SHA256 und
  den SHA256-Hash des CTest-Protokolls.
- Reproduktion:
  `scripts/p2_moving_box_pressure_traction_full_build.cmd`;
  `python scripts/p2_moving_box_pressure_traction_acceptance.py`.
  Nachweise in `ENGINE TEST STUBE/runs/p2_moving_box_pressure_traction_*.{txt,json}`.

## Verbindliche Grenzen

Dies ist **eine diskrete, stückweise konstante Druckoberflächenintegration**
für einen vorgegeben bewegten, achsenparallelen Quader. Der Gegenimpuls
wird **nicht** auf Fluid oder Körper angewendet. Es gibt weder eine
dynamische Festkörpergleichung noch eine nachgewiesene globale
Impuls-/Energieerhaltung; die Vorzeichenidentität der geometrischen
Arbeit ist zunächst eine diskrete Buchhaltung. Eine vollständig
gekoppelte 3D-Impulstensor-Diskretisierung, Wasser-Luft-Spannungen,
freie Oberflächen, externe CFD-Messvalidierung und UE5.8-Native-E2E
dieses Pfades sind offen. Der Live-Solver bleibt unverändert.
