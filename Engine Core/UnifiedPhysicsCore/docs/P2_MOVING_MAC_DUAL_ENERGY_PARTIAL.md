# P2 Teilabnahme: numerische Energie-Stabilitaet auf bewegten MAC-Dualvolumina

Stand: 28.10.2026. **P2 PARTIAL; P3-P8 weiterhin PARTIAL.**

## Abgegrenzter Implementierungsumfang

`P2MovingMacDualEnergy3D::step` verwendet den vorhandenen gemeinsamen
3D-Primal-MAC-Praediktor und den konservativen Upwind-Transport auf drei
bewegten gestaffelten Dualgittern. Fuer jede Komponente werden aus den
exakten offenen Dualvolumina die diskreten Groessen berechnet:

- `P_a = rho * sum(V_old * u_old)` und `P_a' = rho * sum(V_new * u_new)`;
- `K_a = rho/2 * sum(V_old * u_old^2)` und `K_a' = rho/2 * sum(V_new * u_new^2)`;
- `D_a = K_a - K_a'` als **rein numerischer Upwind-L2-Verlust**.

Bei geschlossenem Rand, GCL und Donor-CFL <= 1 ist die
Upwind-Aktualisierung eine volumen-gewichtete konvexe Kombination.
Daraus folgt per Jensen `K_a' <= K_a` fuer den signierten
passiven Komponentenwert. Die Implementierung verweigert die
Zustandsrueckgabe bei einer relevanten Verletzung der numerischen
Impuls- oder Energieinvariante. Eingaben werden nicht mutiert.

**Keine physikalische Energieerhaltung:** Die drei Dualprojektionen
bleiben voneinander unabhaengig. Der passive Transport liefert
weder eine gemeinsame tensorielle Navier-Stokes-Impulsbilanz noch
Druck-/Wandarbeit oder einen Fluid-Festkoerper-Gegenimpuls.
Die Einheiten J und kg*m/s sind nur fuer die definierten
konstantdichten Dual-Kontrollvolumen-Proxies sinnvoll.

## Nachweise

- MSVC-Release-Build und vollstaendiger 75-Suites-CTest:
  `scripts/p2_moving_mac_dual_energy_full_build.cmd`.
- Native unabhängige Nachrechnung jeder offenen Dualzelle,
  Vorher-/Nachher-Impuls, Vorher-/Nachher-Energie, Verlust,
  Nullfeld-Referenz und Negativfaelle:
  `tests/p2_moving_mac_dual_energy3d_tests.cpp`.
- 8^3/12^3, drei Bewegungsachsen, beide Vorzeichen, uniforme
  und nichtuniforme signierte Werte, je zwei aufeinanderfolgende
  Schritte = 48 numerische Referenzzeilen.
- Quantitative Auswertung, Quellhashes und CTest-Loghash:
  `scripts/p2_moving_mac_dual_energy_acceptance.py` und
  `ENGINE TEST STUBE/runs/p2_moving_mac_dual_energy_partial.json`.
- Erster CTest schlug fehl, weil der Test bitgenaue Gleichheit bei
  stationaerem Nullfluss annahm. Die Druckprojektion hat kleine
  numerische Rundungsreste; nach Anpassung auf 1e-10-Toleranz
  wurde die vollstaendige Testsuite erfolgreich wiederholt.

## Weiterhin offene Vollphasen-Kriterien

Ein gemeinsamer physikalischer Impulsflusstensor, dynamische
Fluid-Festkoerper-Gegenreaktion, freie Wasseroberflaechen,
unabhaengige 3D-CFD-Messdaten, globale thermo-mechanische
Energiebilanz und UE5.8-Native-E2E des voll gekoppelten Pfads
sind nicht nachgewiesen. Bestehender Live-Solver unveraendert.
