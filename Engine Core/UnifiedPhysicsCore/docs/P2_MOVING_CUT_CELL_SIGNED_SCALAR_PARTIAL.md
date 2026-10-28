# P2 Teilnachweis: konservativer signierter Skalartransport in bewegten 3D-Cut-Cells

Stand: 28.10.2026. **P2 bleibt PARTIAL. Keine vollstaendige CFD-/FSI-Abnahme.**

## Implementierung

`include/upc/p2_moving_cut_cell_scalar_transport3d.hpp` fuehrt einen
opt-in, zellzentrierten, signierten Skalar mit den **tatsaechlich projizierten**
3D-MAC-Volumenfluessen `q` eines verschobenen Quaders fort.
Der geometrische Open-Volume-Anteil `alpha` stammt aus
`P2MovingBoxCutCellProjection3DBuilder`.

Pro Zelle und Schritt gilt mit Upwind-Donor `s_d`:

`alpha_neu * s_neu = alpha_alt * s_alt - sum(outward q*s_d)/dx^3`.

Der Operator prueft die lokale geometrische Erhaltung
`alpha_neu = alpha_alt - div(q)/dx^3`, den akkumulierten
Donor-CFL-Wert, geschlossene Aussenflaechen, Form/Finitheit,
geschlossene Zellen, konserviertes signiertes Integral und
Monotonie. Keine Eingabe wird mutiert. Das Vorzeichen ist beliebig;
dies ist anders als der begrenzte VOF-Wasseranteil.

**Wichtig:** Der Skalar ist zellzentriert und passiv transportiert.
Selbst wenn `s` eine Geschwindigkeitskomponente repraesentiert,
ist dies noch keine physikalisch korrekte staggered-MAC-Impulsadvektion,
keine Druck-/Kraftkopplung und keine bidirektionale FSI.

## Reproduzierbare Nachweise

- `scripts/p2_moving_cut_cell_scalar_full_build.cmd`: MSVC-Release-Build
  und vollstaendige CTest-Regression, protokolliert unter
  `ENGINE TEST STUBE/runs/p2_moving_cut_cell_scalar_full_ctest_20261028.txt`.
- `tests/p2_moving_cut_cell_scalar_transport3d_tests.cpp`:
  8^3/12^3/16^3, drei Bewegungsachsen, beide Vorzeichen,
  zwei aufeinanderfolgende Zeitschritte, voneinander getrennte
  Face-Flux-Referenzrechnung, signierte Linearitaet,
  geometrische Erhaltung konstanter Werte, globales Integral,
  Rollback durch unveraenderte Eingaben und Negativfaelle.
- `scripts/p2_moving_cut_cell_scalar_acceptance.py`: unabhaengige
  analytische Zell-Quader-Schnittvolumen-Referenz fuer das initiale
  signierte Skalarintegral, native Assertions, 70 CTest-Suites und
  SHA-256-Quell-/Lognachweise. Maschinenlesbarer Report:
  `ENGINE TEST STUBE/runs/p2_moving_cut_cell_scalar_partial.json`.

## Offene Arbeiten

Als naechster echter P2-Schritt muss die skalare Advektion in einen
konservativen **MAC-Dualvolumen-Impulstransport** mit bewegter
Cut-Cell-Geometrie ueberfuehrt und mit Druckprojektion,
VOF, Wandreaktion sowie physikalischer Gesamtimpuls- und
Energiebilanz gekoppelt werden. Freie Oberflaechen,
unabhaengige 3D-CFD-Messreferenzen, CoupledMacWater3D-Integration
und UE5.8-Native-E2E fuer diesen Pfad fehlen. P2–P8 PARTIAL.
