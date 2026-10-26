# P2 – Live-MAC/VOF-Viskositaets-Energiebuchhaltung (Teilabnahme)

Stand: 26.10.2026. **P2 bleibt PARTIAL**, P3–P8 ebenfalls nicht voll abgenommen.

## Tatsächlicher Entwicklungsumfang

`include/upc/coupled_water3d.hpp` aggregiert nun den bereits separat getesteten
`MaskedMacMomentumLedger3D` über **alle adaptiven Viskositätsstufen**
eines öffentlichen `CoupledMacWater3D::step()`. Die neue schreibgeschützte
Kopie `last_viscous_diffusion_ledger()` enthält die komponentenweise
Änderung der staggered-Face-Quadratsumme, interne und Wand-Gradientverluste,
Wandarbeit, explizite Euler-Korrektur, Residuen und die Substep-Zahl.
Jeder neue öffentliche Step setzt den Ledger zurück. `nu=0` bleibt
ohne Viskosität und meldet Nullwerte. Der Operator, die Geschwindigkeit,
VOF-Transport und die Druckprojektion wurden nicht umgestellt.

**Grenze:** Das ist ein exakt diskreter, nicht physikalischer
Face-Quadratsummen-Proxy in m²/s², ohne Dualvolumen-/Fluidmassenwichtung.
Er misst weder physikalische Gesamtenergie noch FSI-Impuls,
Grenzflächenspannung oder Temperaturumwandlung. Der vorhandene
`last_viscous_energy_before/after`-Getter bezieht sich bei mehreren
MAC-CFL-Substeps nur auf die letzte Viskositätsstufe; deshalb wird
für die Summe ausdrücklich der neue akkumulierte Ledger verwendet.

## Reale native Tests

`tests/coupled_viscous_energy_ledger3d_tests.cpp` prüft ein 8×8×6
Wasser/solid-VOF-Szenario mit 36 kg, bewegungslosem Festkörper,
aber vorgeschriebener tangentialer Wandgeschwindigkeit 0,75 m/s.
Über zehn öffentliche Solver-Steps werden algebraischer Abschluss,
Rundungsresiduen, Impulsquellen und Übereinstimmung mit dem bisherigen
skalierten Momentum-Proxy geprüft. Verglichen werden angetriebene
Wand, stationäre Wand und inviskoser Kontrollsolver; die VOF-Masse
bleibt konstant. Bei hohem nu wird adaptive interne Substep-Akkumulation
geprüft, bei nu=0 die Rücksetzung. Die Simulation enthält **keinen**
bewegten Körper oder FSI-Gegenimpuls.

Der erste Native-Build scheiterte ohne MSVC-Developer-Umgebung an
`algorithm`-Include. Mit dem vorhandenen VS18 `vcvars64.bat` wurde
der Build erfolgreich ausgeführt. Der erste neue Test fand eine zu
strikte Fließkomma-Gleichheit `mass==36`; korrigiert auf eine
Toleranzprüfung von 1e-10. Danach **697 native Assertions PASS**.
Die Ausgabe meldete: `mass_kg=36.000000000000007`,
`wall_work=2.8622854929466457`,
`gradient_loss=2.437429725141957`,
`viscosity_substeps=24`.

Reproduzierbar im MSVC-Developer-Terminal:
```powershell
cd "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
cmake --build build --config Release --parallel 5
ctest --test-dir build --output-on-failure --parallel 5
C:\Python314\python.exe scripts\p2_live_energy_acceptance.py
C:\Python314\python.exe scripts\phase_gate.py
```

Maschinenlesbarer Nachweis:
`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_live_energy_partial.json`.
Das Gate verifiziert die neuen nativen Checks, CTest, Quell- und
Binär-SHA256, `phase_complete=false` und den konservativen Phase-Gate-Status.

## Verbleibende P2-Lücken

Spannungskonsistente freie Wasser/Luft-Grenzen, bewegliche Festkörper
mit tatsächlicher Impuls- und Energierückgabe, unabhängige gemessene
3D-CFD-Referenzen und Konvergenz, globale gekoppelte Energie-/Impulsbilanz
sowie eine erneute UE5.8-Native-E2E-Abnahme **für diese Änderung**.
Ein grünes numerisches Teil-Gate schließt P2 nicht ab.
