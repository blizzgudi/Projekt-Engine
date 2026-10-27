# P2 Teilabnahme: kinematischer 3D-MAC-Druck -> traeger Starrkoerper-Praediktor

Stand: 27.10.2026. **P2 bleibt PARTIAL, P3-P8 bleiben PARTIAL.**

## Neu implementiert

- include/upc/lagged_kinematic_pressure_body3d.hpp verbindet die reale
  KinematicWallMacProjection3D mit KinematicWallPressureProbe3D und
  RigidPressureResponse3D.
- Das **alte** lineare und angulare Koerpertempo setzt die Normalgeschwindigkeit
  auf benetzten MAC-Grenzflaechen; eine konvergierte 3D-Projektion erzeugt
  Druck. Dieser liefert Kraft und Drehmoment fuer einen neuen
  Starrkoerper-Zustand inklusive Schwerpunkt-Praediktion.
- Die MAC-Berechnung arbeitet auf einer Kopie. Bei unterkonvergiertem Druck,
  ungueltigen Masken oder ungueltigem Koerperzustand wird kein Fluidzustand
  publiziert. Bei Erfolg wird das projizierte MAC-Feld uebernommen.
- Getrennte Bilanzen fuer **alte Wandgeschwindigkeit** und
  **mittlere Starrkoerpergeschwindigkeit** zeigen den Lag-Fehler explizit.

## Analytische unabhaengige Referenz

Zwei getrennte offene 3D-Wasserzellen zwischen Koerper und
Null-Druck-Luftauslass. Gitter: 8^3, 12^3, 16^3.
dx=0.08 m, rho=1000 kg/m^3, dt=0.025 s, m=4 kg,
I_z=0.2 kg m^2, Schwerpunkt y=0.04 m ueber der Kraftlinie.
Geschwindigkeiten v_x=+0.12, -0.12, 0 m/s.

- Zell-Drucke: p_left=-rho*dx*v/(2*dt), p_right=+rho*dx*v/(2*dt).
- Kraft: F_x=-rho*dx^3*v/dt; fuer +0.12: -2.4576 N.
- Drehmoment: tau_z=0.04*F_x; fuer +0.12: -0.098304 Nm.
- Koerperimpulse: J_x=F_x*dt=-0.06144 Ns;
  L_z=tau_z*dt=-0.0024576 Nms.
- Geschwindigkeitsantwort: v_x'=v_x+J_x/m=0.10464 m/s;
  omega_z'=L_z/I_z=-0.012288 rad/s.
- Altes Wandarbeitsintegral: W_old=F_x*v_x*dt=-0.0073728 J.
- Traegheits-Arbeit mit Mittelgeschwindigkeit:
  W_mid=F_x*(v_x+v_x')*dt/2+tau_z*(omega_z+omega_z')*dt/2
  =-0.0068858413056 J.
- **Nicht geschlossene alte Wandarbeit:** W_mid-W_old
  =0.5*(J_x^2/m+L_z^2/I_z)=+0.0004869586944 J.
  Dies ist ein nachgewiesener Lag-Fehler, **keine** Fluidkorrektur.
- Analytische Referenz nur fuer zwei vereinfachte Ein-Zellen-Taschen;
  keine externe 3D-Messvalidierung.

## Test- und Nachweispfad

- Native MSVC C++17: tests/p2_lagged_kinematic_pressure_body3d_tests.cpp.
  Neun Gitter-/Vorzeichenfaelle, 190 Assertions, drei Gitteraufloesungen,
  Divergenz, Nullbewegung und negative Atomizitaetsfaelle.
- Vollstaendiger CTest-Nachweis:
  ENGINE TEST STUBE/runs/p2_lagged_pressure_body_ctest_20261027.txt.
- Unabhaengige numerische Python-Pruefung und SHA-256:
  scripts/p2_lagged_pressure_body_acceptance.py und
  ENGINE TEST STUBE/runs/p2_lagged_pressure_body_partial.json.
- Das Gate prueft weiterhin P0/P1 PASS und P2-P8 PARTIAL.

## Explizite Grenzen

**Keine vollstaendige dynamische FSI:** Die Koerpermaske wird NICHT
verschoben, es gibt keine swept volumes, keine Rueckwirkung des
Koerperimpulses auf das MAC-Feld und keine gemeinsame Erhaltung von
Fluid- und Starrkoerperimpuls/Energie. Der berechnete Gegenimpuls im
RigidPressureImpulse3D ist nur Buchhaltung. Das Fluid bleibt auf
der alten kinematischen Wandgeschwindigkeit projiziert, waehrend die
Koerpergeschwindigkeit als Praediktor fortgeschrieben wird. Die
Vereinfachung ist **nicht** fuer eine Vollphasenfreigabe geeignet.
Es fehlen externe 3D-CFD-Messdaten, spannungskorrekte freie
Oberflaeche und erneute UE5.8-Native-E2E-Abnahme.
