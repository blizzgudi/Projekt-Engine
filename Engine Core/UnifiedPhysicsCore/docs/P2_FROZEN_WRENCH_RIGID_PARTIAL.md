# P2-Teilabnahme: nichtlineares 3D-Druckmoment als gefrorene Starrkoerper-ODE (30.10.2026)

## Abgegrenzter Implementierungsschritt

`include/upc/p2_frozen_wrench_rigid_step3d.hpp` verbindet zwei bereits
getrennt gepruefte Rechenwege: `P2PressureWrenchReference3D::audit`
ermittelt Druckkraft und -drehmoment an einer **vorgegebenen** fraktionalen
3D-Box; `RigidPressureResponse3D::step` integriert die lineare und
angulare Geschwindigkeit einer Koerper-ODE mit diagonaler, konstanter
Welttraegheit. Der neue Adapter transformiert das Drehmoment vom
beliebigen Druck-Referenzpunkt zur alten Koerpermitte:
`M_body = M_ref + (reference - centre_old) x F`.
Eine von der Box abweichende Anfangs-Koerpermitte wird abgelehnt.

Der gemessene Druck ist ueber den ODE-Schritt eingefroren. Das
kinematisch vorgegebene Druck-Boxziel ist **nicht** das integrierte
Koerperziel; die euklidische Differenz wird explizit ausgegeben.
Die aus dem Druck abgeleitete Koerperdynamik wird nicht in eine
MAC-Geschwindigkeit, VOF-Maske oder Koerperorientierung zurueckgeschrieben.

## Tests und reproduzierbare Nachweise

- Native MSVC-Release-C++17-Probe: `tests/p2_frozen_wrench_rigid_step3d_tests.cpp`.
  Vier Faelle: 8^3 und 12^3, je beide Translationsrichtungen.
  Nichtlineares Druckfeld mit echtem intrinsischem Drehmoment.
- Analytische ODE-Referenz: `v_1=v_0+F*dt/m`,
  `omega_1=omega_0+M*dt/I`, Trapez-Verschiebung und
  `Delta K = F*Delta x + M*Delta theta` bei konstantem Wrench.
  Unabhaengige Python-Referenz integriert die fraktionalen
  Druckflaechen explizit, einschliesslich Referenzpunktwechsel.
- Native Negativfaelle: inkonsistente Koerpermitte, Nulltraegheit,
  nichtfinite Winkelgeschwindigkeit, negatives Zeitintervall.
  Plus Gauge-Invarianz bei +500 Pa, zwei Referenzpunkte und
  explizit ungleiche kinematische/gefrorene ODE-Trajektorien.
- `scripts/p2_frozen_wrench_rigid_full_build.cmd` kompiliert die
  Standalone-Probe und fuehrt die bestehende 87er CTest-Suite aus.
- `scripts/p2_frozen_wrench_rigid_acceptance.py` prueft alle
  gemeldeten nativen Zahlen gegen separat berechnete Referenzen,
  erfasst SHA-256 der Quellen, nativen Binaerdatei und CTest-Logs.
  Bericht: `ENGINE TEST STUBE/runs/p2_frozen_wrench_rigid_partial.json`.
  CTest-Log: `p2_frozen_wrench_rigid_full_ctest_20261030.txt`.

## Physikalische Grenzen

**P2 bleibt INCOMPLETE, P3-P8 ebenfalls.** Dieser Schritt weist
lediglich die Druckmoment-Uebergabe und die ODE-Energieidentitaet bei
**gefrorener** Drucklast nach. Er beweist keine fluidseitige
Gegenimpulsanwendung, keine Gesamtenergieerhaltung, keine rotierende
Kollision/Orientierung, keine spannungskorrekte freie Oberflaeche,
keine unabhaengige gemessene 3D-CFD-Validierung und keine neue
Unreal-5.8-Native-E2E-Abnahme. Der bestehende P8-Prototyp bleibt
ein separater, nicht auf diese neue Kopplung uebertragbarer Nachweis.
