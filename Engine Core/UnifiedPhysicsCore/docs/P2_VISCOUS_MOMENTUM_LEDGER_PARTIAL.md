# P2 – Masked-MAC-Viskosität: diagnostische Wand-Impulsbilanz (Teilabnahme)

Stand: 26.10.2026. **P2 = PARTIAL**, keine Vollabnahme.

## Umsetzung

Der bereits gekoppelte `CoupledMacWater3D`-Solver und der maskierte
`ViscousMacDiffusion3D`-Operator erhalten einen optionalen
`MaskedMacMomentumLedger3D`-Diagnosekanal. Pro explizitem
Diffusions-Substep und MAC-Achse werden die Änderungen der
**aktiven staggered Face-Geschwindigkeitssumme** und ihre diskreten
Quellen durch statische Festkörpergrenzen und äußere Null-Geschwindigkeits-
Ränder getrennt summiert. Die Differenz wird als Abschlussresiduum
gespeichert. Die 3D-Wasser-/VOF-Masse wird nicht verändert.

Die Live-Schnittstelle stellt die über den gesamten `step()`-Aufruf
akkumulierten Größen mit dem Faktor `rho*dx^3` als
`*_proxy_kg_m_s` bereit. Bei `nu=0` werden die Ledger explizit
zurückgesetzt. Es gibt keine neue Laufzeitabhängigkeit und der
inviskose Codepfad bleibt erhalten.

**Wichtig:** Das ist eine *diskrete numerische Face-Summenbilanz*, kein
physikalisch korrektes Kontinuumsimpulsintegral. MAC-Face-Dualvolumen,
Flächenkräfte, reale Schubspannung, Druck, Advektion und Gravitation
werden hier **nicht** zu einer globalen physikalischen Bilanz
zusammengeführt. Die Gegenreaktion wird keinem beweglichen Festkörper
zugeführt. Eine positive Summe ist keine nachgewiesene Körperkraft.

## Unabhängige quantitative Probe

Festes 10×10×10-Voxel-Volumen mit `dx=0.1 m`,
`nu=0.012 m²/s`, `dt=0.03 s` und
`alpha=nu*dt/dx²=0.036`. Festkörper belegt `x<4`,
Wasser `x>=4`. Die vorgeschriebene Tangentialgeschwindigkeit
ist `v_wall_y=0.4 m/s`.

Es gibt exakt `10 z-Scheiben * 9 innere y-Faces = 90`
angrenzende Flüssigkeits-Faces. Ein unabhängiges diskretes
Einzelschritt-Stencil ergibt für die Tangentialkomponente:

`sum(delta_v_y)=90 * 0.036 * 0.4 = 1.296 m/s`.

Der Test verifiziert diese Zahl, das Vorzeichen bei umgekehrter
Wandgeschwindigkeit, den Null-Normalimpuls, den geschlossenen
aktiven Face-Summen-Operator, adaptives Substepping,
`nu=0`-Identität, Invalid-Input-Fail-Closed und die
Live-Integration in einen 8×8×6-VOF-Tank mit 36 kg Wasser.
Der Test setzt **keine** falschen Vollphasenflags.

## Nachweis und Reproduktion

Aus `D:\Projekt Engine\Engine Core\UnifiedPhysicsCore`:

1. MSVC-Developer-Umgebung (`vcvars64.bat`) aktivieren.
2. `cmake --build build --config Release -j 5`.
3. `ctest --test-dir build --output-on-failure`.
4. `C:\Python314\python.exe scripts\p2_momentum_ledger_acceptance.py`.

Native-Test: `tests/viscous_momentum_ledger3d_tests.cpp`.
Maschinenlesbares Teil-Gate:
`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_viscous_momentum_ledger_partial.json`.
Das Gate speichert Exitcodes, Zahlen, CTest-Stand,
`phase_gate.py`-Status, Testbinary-Hash und Quell-SHA256.
`phase_complete=false` ist unveränderliche Teilabnahme-Semantik.

## Weiter offen für P2

Physikalische, spannungskonsistente 3D-Viskosität an dynamischen
freien Oberflächen, bewegte Festkörper mit echten
Kraft-/Impuls-Rückwirkungen, unabhängig gemessene 3D-Referenzfälle,
globale Impuls-/Energieerhaltung, räumliche Konvergenz und
erneute Native-Unreal-5.8-E2E-Abnahme dieser Erweiterung.
P3–P8 bleiben ebenfalls PARTIAL.
