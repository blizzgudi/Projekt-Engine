# P2 Teilabnahme: stage-resolved 3D MAC face energy proxy (31.10.2026)

**Status: PARTIAL. P2–P8 INCOMPLETE; ALL_COMPLETE=False.**

## Implementierung

Die optionale `CoupledMacWater3D::enable_face_energy_stages(true)`
aktiviert einen read-only Energieproxy der staggered MAC-Geschwindigkeiten
`0.5 * rho * dx^3 * sum(u_face^2 + v_face^2 + w_face^2)`.
Alle Faces (einschließlich trockener und Rand-Faces) erhalten dabei die
gleiche nominelle Zellmasse; das ist **keine** physikalisch korrekte
VOF-gewichtete kinetische Energie. Standardmäßig ist die Diagnose
deaktiviert, um zusätzliche Face-Scans zu vermeiden.

`last_face_energy_stages()` akkumuliert über alle adaptiven Substeps
getrennte Änderungen aus Geschwindigkeitsadvektion, Viskosität,
Gravitation, erster Druckprojektion, VOF-Transport und optionaler
zweiter Druckprojektion. Außerdem werden Start-/Endwert, Substeps und
algebraischer Abschlussrest ausgegeben. Der VOF-Transport verändert
nur die Zellbelegung, daher ist sein direkter Face-Energiebeitrag null.
Die vorherige VOF-Gravitationspotenzialbilanz bleibt separat erhalten.

Im transaktionalen `step_verified_reprojected` bleibt der Ledger bei
gescheiterter Projektion zusammen mit dem gesamten Zustand unverändert.
Das Instrumentieren verändert weder die Drucklösung noch die
Wasserverteilung; die deaktivierte Variante dient als Paritätsreferenz.

## Native Nachweise

MSVC-Release-Build, 93/93 CTest-Suites und 4.524 native Assertions PASS.
Zwei 3D-Gitter (8³, 12³), jeweils zwei vertikal benachbarte volle
Wasserzellen, dx=0,1 m, rho=1000 kg/m³, dt=0,01 s.

Die unabhängig analytisch berechnete Gravitationsänderung beträgt
`0.5 * rho * dx^3 * 3 * (9.81*dt)^2 = 0.014435415 J`:
drei vertikale MAC-Faces werden aus Ruhelage um -g*dt beschleunigt.
Gemessener Gravity-Stage-Proxy: 0,014435415000000002 J.
Die erste Druckprojektion ändert hier den Proxy nicht; die zweite
Nachprojektion senkt ihn um 0,000041185284665097924 J.
Endwert: 0,014394229715334904 J. Der algebraische Rest ist 0,
finale Nasszellendivergenz: 3,2213814948889308e-13 s^-1.

Tests prüfen zusätzlich bitgenaue VOF-/Druck-Parität bei deaktivierter
Diagnose, Substep-Zahl, unveränderten Ledger bei erzwungener
Unterkonvergenz und Reset beim Abschalten. Reproduktion:
`scripts/p2_face_energy_stages_full_build.cmd`, anschließend
`python scripts/p2_face_energy_stages_acceptance.py`.
Maschinenlesbarer Nachweis mit SHA-256:
`ENGINE TEST STUBE/runs/p2_face_energy_stages_partial.json`;
CTest-Log: `p2_face_energy_stages_ctest_20261031.txt`.

## Physikalische Grenzen

Der Nullrest belegt **nur** die algebraische Partition eines numerischen
Face-Proxys. Er ist kein Beweis für globale Energieerhaltung.
Insbesondere fehlen VOF-gewichtete kinetische Energie, echte Druckarbeit,
viskose Wärme, Energiefluss über bewegte Festkörper, Gegenimpulse,
freie-Oberflächen-Spannungen, externe 3D-CFD-Messvalidierung und
Unreal-5.8-Native-E2E dieser Erweiterung. P2 bleibt offen.
