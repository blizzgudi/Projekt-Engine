# P2 Teilabnahme: Uniform-Cell Finite-Volume VOF Inertia

Stand 31.10.2026. **Nur abgegrenzter Diagnosebaustein, keine P2-Vollabnahme.**

## Physikalische Annahme und Änderung

Die bestehende VOF-Drehimpulsdiagnostik behandelt Wasser pro Zelle als Punktmasse im Zellzentrum. Der neue **read-only** Adapter `vof_uniform_cell_inertia3d` nimmt stattdessen für die geometrische Trägheit eine **homogene Massenverteilung innerhalb jedes kubischen Voxels** an. Für Füllgrad `f` ist die effektive Dichte in dieser Zelle `f*rho`, auch bei `0<f<1`. Das ist **keine Rekonstruktion der echten Teilfüllungsgeometrie**.

Für jede Zelle der Kantenlänge `dx` und Masse `m=f*rho*dx^3` gilt zusätzlich zur Punktmassen-Näherung der intrinsische Trägheitstensor `I_cell = (m*dx^2/6) * identity`. Über alle Zellen ist die Korrektur daher `M*dx^2/6` auf der Diagonale, sowohl um den gewählten Ursprung als auch um den Schwerpunkt. Massenmittelpunkt, linearer Impuls und Drehimpuls ändern sich nicht. Die ursprüngliche API bleibt unverändert und weiterhin verfügbar.

## Unabhängige Referenz

Die neue Native-Testdatei integriert `I_ij = integral rho*(r^2 delta_ij-r_i*r_j) dV` über **acht 2x2x2 Gaußpunkte je Zelle**. Die Quadratur referenziert keine intern berechnete Korrektur und ist für den quadratischen Integranden exakt. Sie prüft zwei asymmetrische Teilfüllungsfelder auf 8^3 und 12^3, verschiedene Ursprünge, Schwerpunkt, Nebendiagonalen, fehlerhafte Eingaben und den Leerzustand.

Zusätzlich wird ein **physisch gleich großer, vollständig gefüllter Würfel** mit Seitenlänge 0,2 m und Dichte 1000 kg/m³ auf 2^3, 4^3 und 8^3 gefüllten Zellen repräsentiert. Die unabhängige Kontinuumsreferenz ist `M=8 kg`, `I_COM=M*L^2/6=0.05333333333333334 kg m^2`. Der korrigierte Tensor soll für jede Rasterauflösung gleich sein; die Punktmassenvariante unterschätzt ihn.

## Reproduktion und Status

1. `scripts\p2_vof_uniform_cell_full_build.cmd` startet den echten MSVC-Release-Build und die gesamte CTest-Suite und schreibt Logs ausschließlich in `ENGINE TEST STUBE\runs`.
2. `python scripts\p2_vof_uniform_cell_acceptance.py` führt die Native-Referenzen erneut aus, prüft quantitative Werte und schreibt `ENGINE TEST STUBE\runs\p2_vof_uniform_cell_partial.json` mit Quell-, Binär- und Protokoll-SHA256.
3. `python scripts\phase_gate.py` entscheidet über den **gesamten** Phasenstatus; diese Teilabnahme setzt keinen Vollstatus.

**Grenzen:** Kein Unterzellen-Freiflächenprofil, keine lokalen Wirbel-/Spinmomente, kein dynamisches rotierendes Zweiwege-FSI, keine global konservative Impuls-/Energieübertragung, keine unabhängigen 3D-CFD-Messdaten und keine neue UE5.8-Native-E2E-Abnahme. **P2–P8 bleiben offen.**
