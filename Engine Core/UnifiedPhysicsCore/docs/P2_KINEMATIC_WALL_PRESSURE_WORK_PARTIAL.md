# P2 PARTIAL — Drucktraktion und Wandarbeit der kinematischen 3D-MAC-Wand (27.10.2026)

## Abgrenzung

Neuer read-only-Operator KinematicWallPressureProbe3D::measure in
include/upc/kinematic_wall_pressure_work3d.hpp. Er verarbeitet das
pressure_pa-Ergebnis der tatsächlich ausgeführten
KinematicWallMacProjection3D::project-Projektion (nicht den Druckzustand
von MacProjection3D, der von diesem separaten Projektor nicht geschrieben wird).

Für jede benetzte Grenzfläche eines eingefrorenen Voxel-Körpers:
- Druckkraft auf Körper: dF = -p * n_solid_to_liquid * dx^2.
- Druckmoment um centre_m: dTau = r x dF.
- Lokale Wandgeschwindigkeit: u_wall = v + omega x r.
- Momentanleistung: dP = dF dot u_wall.
- Für konstante Lasten im eingefrorenen Intervall:
  W = P * dt, J = F * dt, L = Tau * dt.

Das sind physikalisch dimensionierte Drucktraktions-Diagnosen an
achsenparallelen Voxel-Faces, keine vollständig stresskonsistente
Free-Surface- oder Fluid-Solid-Simulation. Keine MAC- oder Solid-Mutation.

## Unabhängige Referenzen und echte Tests

- Zwei voneinander getrennte offene 1-Zellen-Wasserkammern links/rechts
  einer festen 2-Zellen-Solid-Wand, rho=1000 kg/m^3, dx=0.08 m,
  dt=0.025 s, v_x=+/-0.12 m/s.
- Aus der diskreten Einzellen-Poisson-Gleichung mit Air-Dirichlet:
  p_left=-rho*dx*v/(2*dt)=-192 Pa, p_right=+192 Pa.
- Unabhängig berechnete Kraft: F_x=-2*192*0.08^2=-2.4576 N.
  Leistung P=F_x*0.12=-0.294912 W.
  Arbeit W=-0.0073728 J, Körperimpuls -0.06144 Ns.
- Negative Geschwindigkeit: umgekehrte Kraft, gleiche negative
  Druckarbeit. Stationäre Wand: Nullwerte.
- Geschlossener vollständig benetzter 2x2x2-Körper mit 24 Faces:
  additive +73 Pa-Druckeichung ändert weder Gesamtkraft, Drehmoment
  noch Leistung. P = F dot v + Tau dot omega separat geprüft.
- Negativfälle: NaN-Druck, falsche Arraylänge, Wet/Solid-Überlappung,
  fehlende Körpermaske.

Native Test: tests/p2_kinematic_wall_pressure_work3d_tests.cpp.
CTest: ENGINE TEST STUBE/runs/p2_kinematic_pressure_work_ctest_20261027.txt.
Gate: python scripts/p2_kinematic_pressure_work_acceptance.py.
Report: ENGINE TEST STUBE/runs/p2_kinematic_pressure_work_partial.json.

MSVC Release: 49/49 CTest PASS; 69 native Assertions.
Das Python-Gate prüft unabhängig analytische Zahlen, Source-/Binary-/Log-
SHA256 und den konservativen Phasenstatus. Der tatsächlich erzeugte
Gate-Report ist für die Ausführung maßgeblich.

## Offen / kein P2-Vollabschluss

Geometrie eingefroren. Körperimpuls nicht auf dynamischen Körper
übertragen; kein konservativer Gegenimpuls nach MAC-Reprojektion.
Keine Swept-Volume-Geometrie, kein freier Oberflächen-Spannungstensor,
keine externen gemessenen 3D-Strömungsreferenzen, keine globale
Energie-/Impulsbilanz und keine neue UE5.8-Native-E2E-Abnahme.
P2 bleibt PARTIAL; P3–P8 bleiben vollumfänglich offen.
