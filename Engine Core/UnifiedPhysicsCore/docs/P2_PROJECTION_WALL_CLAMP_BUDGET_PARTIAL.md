# P2 Teilabnahme: Druckprojektion und MAC-Wand-Clamp-Bilanz (30.10.2026)

**Keine P2-Vollabnahme.** Der bestehende `ProjectionGradientBudgetAudit3D`
bilanzierte nur freie Wasser/Wasser- und Wasser/Luft-Faces. Die
`MacProjection3D::project`-Implementierung setzt aber vor dem Drucksolve
auch alle äußeren sowie festkörperberührenden MAC-Faces auf exakt null.
Diese numerischen Impuls- und Energieänderungen wurden zuvor nicht erfasst.

## Implementierung

`ProjectionGradientBudget3D` erfasst nun separat:
`clamp_linear_Ns`, `clamp_angular_Nms`,
`clamp_face_energy_delta_proxy_J`, die Anzahl der internen
Festkörper- und äußeren MAC-Faces und den größten Restwert der
projizierten Wandgeschwindigkeit. Eine Face wird genau einmal gezählt;
die Druckgradientenbilanz wird nicht mit dem Clamp vermischt.
Dies ist ein **numerisches Staggered-Face-Proxy-Ledger**, keine
physikalische Kraft-, Impuls- oder Gesamtenergieerhaltung.

## Unabhängige Referenz

Eine ruhende 3D-Wasserzelle, ein benachbarter Festkörper und trockene
Nachbarn werden auf 8³ und 12³ getestet. Drei vorgegebene
Face-Geschwindigkeiten (Wasser/Festkörper +0,25 m/s, Festkörper/Luft
-0,17 m/s, Außenrand +0,31 m/s) müssen von der Projektion auf null
gesetzt werden. Alle übrigen Faces sind null, daher ist die analytische
Druckkorrektur ebenfalls null.

Für jedes dieser Faces gilt unabhängig vom C++-Audit:
`delta_p=-rho*dx^3*v_before`,
`delta_E=-0.5*rho*dx^3*v_before^2`,
`delta_L=r cross delta_p`.
Es werden 6 interne Festkörperfaces und `6*n*n` Außenfaces erwartet.
Die Python-Abnahme berechnet alle drei Impuls- und Drehmomentkomponenten
sowie Energie und Face-Zählungen unabhängig.

Reproduktion: `scripts/p2_projection_wall_clamp_full_build.cmd`
(MSVC Release + alle CTests), danach
`python scripts/p2_projection_wall_clamp_acceptance.py`.
Versionierter Report: `ENGINE TEST STUBE/runs/p2_projection_wall_clamp_partial.json`.
Das Gate `scripts/phase_gate.py` muss weiterhin P2 als INCOMPLETE melden.

## Grenzen

Die Diagnose betrifft nur statische Nullgeschwindigkeits-Wände und
numerische Face-Massen `rho*dx^3`; eine physikalische FSI-Kraft kann
daraus nicht abgeleitet werden. Nicht nachgewiesen: bewegliche/rotierende
Körpergeometrie mit konsistenter Druckrandbedingung, reale
Wandstressintegration, vollständige Impuls- und Energiebilanz,
unabhängige 3D-CFD-Messdaten und UE5.8 Native-E2E für diese Erweiterung.
