# P2 Teilabnahme: Druckarbeit auf drei bewegten MAC-Dualgittern

Stand: 28.10.2026. **P2 PARTIAL, P3-P8 PARTIAL, ALL_COMPLETE=False.**

## Implementierung

Die optionale, rein lesende C++-Komponente
`P2MovingMacDualPressureWork3D::audit` projiziert einmal ein gemeinsames
primal gestaffeltes MAC-Geschwindigkeitsfeld auf die geometrische
Kontinuitaet eines bewegten Quaders. Sie rekonstruiert daraus die
Praediktoren aller drei um eine halbe Zelle verschobenen MAC-Dualgitter.
Die bereits vorhandenen separaten Dual-Druckprojektionen werden
durchgefuehrt und fuer jede Komponente mit dem bestehenden diskreten
Cut-Cell-Druckarbeits-Audit geprueft.

Pro Dualgitter gilt numerisch:
`K_proj-K_pred = W_geometrisch - K_druckkorrektur`.
Die drei Identitaeten werden einzeln und in Summe kontrolliert.
Die Druckarbeit ist die diskrete, druckgewichtete Volumenaenderung
`sum_i p_i (V_alt,i - V_neu,i)`; die Kinetik ist eine
aperturgewichtete Face-Groesse. Alle Ausgaben sind reine Diagnose.
Das Verfahren erzeugt **keinen gemeinsamen physikalischen Impulstensor**
und wendet weder Wandkraft noch Gegenimpuls auf einen Starrkoerper an.

## Reproduzierbare Nachweise

- `scripts/p2_moving_mac_dual_pressure_work_full_build.cmd`: MSVC
  Release und vollstaendiger CTest (76 Suites).
- `tests/p2_moving_mac_dual_pressure_work3d_tests.cpp`: 8^3 und 12^3,
  je drei Bewegungsachsen und beide Richtungen (12 bewegte Faelle),
  zwei statische Nullreferenzen, alle drei Dualgitter, direkte
  Reprojektion als Vergleich, Summenidentitaeten und Druck-Gauge
  (+17 Pa). Manipulierte Druckgradienten und ungueltige
  Eingaben werden verworfen. Caller-Felder bleiben unveraendert.
- `scripts/p2_moving_mac_dual_pressure_work_acceptance.py` prueft
  die CTest-Vollstaendigkeit, native Assertions, 14 Referenzzeilen,
  unabhaengig in Python die ausgegebenen Energieidentitaeten,
  Gitter-/Achsenabdeckung, Source- und Log-SHA256.
- Nachweise: `ENGINE TEST STUBE/runs/p2_moving_mac_dual_pressure_work_partial.json`
  und `p2_moving_mac_dual_pressure_work_full_ctest_20261028.txt`.
  Die Werte sind **diskrete numerische Referenzen**, keine
  externen CFD-Messdaten.

## Verbleibende Akzeptanzluecken

Die drei Dualdruckkorrekturen sind weiterhin voneinander unabhaengig.
Eine gemeinsame konservative Impulstensor-Diskretisierung, dynamische
Fluid-Festkoerper-Gegenreaktion mit Impuls- und Energieerhaltung,
spannungskorrekte freie Wasseroberflaechen, unabhaengige gemessene
3D-CFD-Validierung und UE5.8-Native-E2E des gekoppelten Pfads
fehlen. Der produktive `CoupledMacWater3D`-Solver bleibt unveraendert.
