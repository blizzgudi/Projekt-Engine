# P2 Teilnachweis: Eigengewicht, hydrostatischer Auftrieb und Potentialarbeit

**Status: PARTIAL; keine vollstaendige P2- oder FSI-Abnahme.**

Der neue optionale `P2HydrostaticGravity3D`-Pfad erweitert die
stratifizierte, ruhende Hydrostatik um das Eigengewicht des starren
Quaders. Der vertikale Kraftansatz ist
`F_net = rho_w*g*A*clamp(h-z_bottom,0,H) - m*g`.
Die Kraft wird fuer einen Zeitschritt am alten Koerperzustand
eingefroren und die Translation mit dem bestehenden
`RigidPressureResponse3D`-Integrator ausgefuehrt. Der bereits
vorhandene Hydrostatikpfad bleibt unveraendert.

**Physikalische Trennung:** Der diagnostische Gegenimpuls des Fluids
ist ausschliesslich `-F_buoyancy*dt`, waehrend `-m*g*dt` als
aeusserer Gravitationsimpuls separat ausgewiesen wird. Der
Rigid-Integrator bilanziert intern die gesamte Nettokraft. Seine
zunaechst auf die Nettokraft bezogene allgemeine
`fluid_reaction_impulse_Ns`-Diagnose wird im neuen Adapter explizit
auf die **reine Auftriebsreaktion** korrigiert. Auch das Feld
`buoyancy_reaction_unapplied_Ns` weist diesen Wert separat aus.
Keine dieser Reaktionen wird auf MAC-Faces angewendet.

**Exakte Potential-Referenz bei fixem Wasserstand:** Fuer den
Koerpermittelpunkt `z`, die Hoehe `H`, den Boden `b=z-H/2`
und `d=h-b` gilt das hydrostatische Potential relativ zur trockenen
Position

- trocken (`d<=0`): `U_b=0`
- teilweise nass (`0<d<H`): `U_b=rho*g*A*d^2/2`
- ganz nass (`d>=H`): `U_b=rho*g*A*(H*(d-H)+H^2/2)`.

Daraus folgt `-dU_b/dz=F_b`. Die Differenz
`Delta K + m*g*Delta z + Delta U_b` misst die Abweichung
zwischen eingefrorener Anfangskraft und exakter Potentialarbeit.
Bei konstantem Auftrieb (trocken oder ganz eingetaucht) verschwindet
sie bis auf Rundung. Bei Bewegung innerhalb einer teilweise
benetzten Oberflaeche ist sie im Test **bewusst ungleich null**:
`rho*g*A*(Delta z)^2/2`. Das ist ein nachgewiesener
Zeitdiskretisierungsfehler, keine Verletzung einer behaupteten
vollstaendigen FSI-Energieerhaltung.

**Native Nachweise:** 8^3 und 12^3, sechs Wasserstaende,
g=0/9,81, insgesamt 24 analytische Faelle, ausserdem neutraler
Auftrieb, Bewegung an der Teilzellen-Wasserlinie, ungültige Eingaben
und geometrische CFL-Grenze. Das Gate dokumentiert Source-SHA256,
CTest-Protokollhash und quantitative Analytik. Alle Tests sind
ausschliesslich auf das optionale Hydrostatik-Modul begrenzt.

**Offen:** Keine dynamische Wasserstands-Rueckkopplung, kein auf
MAC-Fluidmasse angewendeter Gegenimpuls, keine freie Oberflaeche mit
korrekter Spannungsbedingung, keine globale Fluid-/Festkoerper-
Impuls-/Energiebilanz, keine unabhaengigen 3D-CFD-Messdaten und
kein UE5.8-Native-E2E dieses neuen Pfads.

Belege: `tests/p2_hydrostatic_gravity3d_tests.cpp`,
`scripts/p2_hydrostatic_gravity_acceptance.py`,
`ENGINE TEST STUBE/runs/p2_hydrostatic_gravity_partial.json` und
`ENGINE TEST STUBE/runs/p2_hydrostatic_gravity_full_ctest_20261029.txt`.
