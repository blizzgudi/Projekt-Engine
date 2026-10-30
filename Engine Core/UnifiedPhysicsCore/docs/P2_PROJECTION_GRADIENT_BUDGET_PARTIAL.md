# P2 Teilabnahme: Druckgradient als Ursache des Nachprojektionsimpulses

Stand: 30.10.2026. **Kein P2-Vollabschluss.**

Nach dem 6-DOF-Gegenimpuls verändert die zweite MAC-Druckprojektion die
staggered-Face-Impulse. Der neue read-only C++17-Adapter
`ProjectionGradientBudgetAudit3D` rechnet diese Veränderung aus dem
tatsächlich gelösten Druckfeld nach. Für jede aktive innere Wasser-Wasser-
oder Wasser-Luft-Face gilt die diskrete Korrektur

`dv = -dt/(rho*dx) * gamma * (p_right-p_left)`

mit `gamma=1` bei Wasser/Wasser und `gamma=2` bei Wasser/Luft.
Daraus werden lineare und angulare **Face-Massenproxy-Impulse**
(`rho*dx^3*dv`) sowie die exakte diskrete Energieidentität
`delta_E = sum(m*v_before*dv) + sum(0.5*m*dv^2)` berechnet.

Der native Test vergleicht auf 6³/8³/12³-Gittern eine einzelne nasse
Zelle mit sechs freien Oberflächen gegen die unabhängig hergeleitete
3D-Poisson-Lösung `p=-rho*dx*S/(12*dt)`, wobei `S` die Summe
der sechs orientierten Face-Geschwindigkeiten ist. Die sechs Korrekturen
sind jeweils `+S/6` bzw. `-S/6`. Die analytische Änderung des
Face-Energieproxies ist `-rho*dx^3*S^2/12`. Zusätzlich werden zwei
2x2x2-Wasserwürfel mit zwölf Wasser/Wasser- und 24 Wasser/Luft-Faces
auf 8³/12³-Gittern nachprojiziert. Ungültige Eingaben werden abgelehnt.

Nachweise:
- `tests/p2_projection_gradient_budget3d_tests.cpp` (native MSVC)
- `scripts/p2_projection_gradient_budget_acceptance.py` (unabhängige Analytik)
- `ENGINE TEST STUBE/runs/p2_projection_gradient_budget_partial.json`
- `ENGINE TEST STUBE/runs/p2_projection_gradient_budget_ctest_20261030.txt`

**Grenze:** Das ist die algebraische Erklärung der nach der Projektion
auftretenden Impulsänderung. Die verwendete Face-Masse ist keine
kontinuumsphysikalisch nachgewiesene Flüssigkeitsmasse, und der
Druckgradient wird nicht als korrekte physikalische Wandtraktion auf
den bewegten Körper zurückgeführt. Insbesondere werden globale
Impuls-/Energieerhaltung, echte rotierende Körpergeometrie,
externe CFD-Messreferenzen und UE5.8-Native-E2E **nicht** bewiesen.
P2–P8 bleiben INCOMPLETE.
