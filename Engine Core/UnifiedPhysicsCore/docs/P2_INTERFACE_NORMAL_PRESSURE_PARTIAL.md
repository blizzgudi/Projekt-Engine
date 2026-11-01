# P2 Teilnachweis: vorgeschriebener Normaldruck an 3D-Wasser/Luft-Grenzen (01.11.2026)

## Umfang und physikalische Bedeutung

Der native MAC-Jacobi-PCG-Solver kann nun optional **von null verschiedene
flüssigkeitsseitige Druckrandwerte** an jeder der sechs Richtungen einer
Wasser/Luft-Grenzfläche verarbeiten. Die Bedingung ist eine inhomogene
Dirichlet-Vorgabe in Pa (Überdruck/Gauge) und verwendet dieselben geometrischen
Interface-Abstände `theta*dx` wie die vorhandene PLIC-/VOF-Druckprojektion.
Die Poisson-Rechte-Seite erhält `sum(p_boundary/theta)`; die anschließende
MAC-Face-Korrektur verwendet genau denselben Druckrandwert und dieselbe
Grenzdistanz. Andernfalls wäre die Druckgleichung nicht konsistent mit der
tatsächlichen Geschwindigkeitsprojektion.

- `MacProjection3D::set_uniform_interface_pressure_pa` für einen globalen
  Druckwert und `set_interface_pressure_{x,y,z}` für einzelne Faces.
- `clear_interface_pressures` verwirft vor einem neuen Interface-Feld
  sämtliche gespeicherten Face-Werte. Ein nicht zurückgesetztes dynamisches
  Face-Feld wäre physikalisch **stale**; der Aufrufer ist für die Aktualisierung
  verantwortlich. `clear_interface_distances` bleibt unabhängig.
- `CoupledMacWater3D::set_uniform_air_interface_pressure_pa` schaltet
  eine konstante Vorgabe in der **laufenden** MAC/VOF-PCG-Projektion frei,
  auch bei Reprojektion nach Wassertransport. Der Default ist 0 Pa und
  die ältere Gauss-Seidel-Projektion bleibt unverändert.

Das ist **ein notwendiger, aber nicht hinreichender Teil einer
stresskonsistenten Wasser/Luft-Grenzbedingung**. Insbesondere wird weder
`p_liquid - p_air = sigma*kappa + viscous_normal_stress` aus Krümmung,
Oberflächenspannung und viskoser Spannung errechnet noch die tangentiale
Spannung zwischen zwei bewegten Phasen gekoppelt. Der vorgegebene Randdruck
ist **keine** experimentell validierte Luft-/Kapillardynamik.

## Reale Native-Prüfungen

Buildskript: `scripts/p2_interface_normal_pressure_build.cmd` (MSVC
Win64 Release, vorhandener CMake/Ninja-Build, vollständige CTest-Suite).
Ausgabe: `ENGINE TEST STUBE/runs/p2_interface_normal_pressure_{build,native,ctest}.txt`.

1. Ein isoliertes flüssiges 3D-MAC-Zellvolumen mit sechs unabhängig
   vorgeschriebenen Face-Drücken und sechs geometrischen `theta`-Werten
   wird gegen die **extern berechnete finite-volume-Referenz**
   `p_cell=sum(p_face/theta)/sum(1/theta)=159.090909... Pa` geprüft.
   Der Solver braucht genau eine PCG-Iteration; alle sechs
   signierten Face-Geschwindigkeiten und die Divergenz werden gegen
   analytisch berechnete Werte geprüft.
2. Sechs unabhängige achsparallele 6³-Wasser-Slabs (x/y/z und beide
   Normalenorientierungen): 650 Pa konstanter Grenzdruck verschiebt den
   Flüssigkeitsdruck um 650 Pa, ohne neue Geschwindigkeit zu erzeugen.
3. Echte gekoppelte 6×6×8-MAC/VOF-PCG-Simulation einschließlich
   Post-Transport-Reprojektion: 144 kg Wasser bleiben erhalten; ein
   konstanter 650-Pa-Grenzdruck verschiebt die Drucklösung, nicht die
   VOF-Massenbewegung. Gemessene Divergenzen: 8.5279e-14 s^-1 (0 Pa)
   und 1.29621e-10 s^-1 (650 Pa).
4. Ungültige Face-Indizes, NaN/Inf, Versuch einer globalen Druckänderung
   bei aktiven Face-Overrides und ein Live-Aufruf ohne PCG werden
   ausdrücklich abgewiesen.

**Ergebnis:** 4.785 Assertions im neuen nativen Test PASS; **127/127
CTest-Suites PASS**. Das unabhängige Skript
`scripts/p2_interface_normal_pressure_acceptance.py` prüft den
Native-Log erneut, führt das native Binary separat aus, prüft
Quell-/Binary-/Log-SHA256 und verwirft bewusst manipulierte Protokolle.
Der maschinenlesbare Bericht liegt unter
`ENGINE TEST STUBE/runs/p2_interface_normal_pressure_partial.json`.

## Negative Messlage / noch offen

Die bereits vorhandene echte 6,00-s-SPHERIC/MARIN-Messvalidierung auf
32×10×10 bleibt **FAIL**: H1–H4-RMSE = 0,136525 / 0,091500 /
0,148245 / 0,224399 m (Limit jeweils 0,05 m). P1–P8-Messdruck-
Konvergenz, unabhängige vollständige 6-s-Dreigitterrechnungen,
stresskonsistente Oberflächenspannung und Viskosität, Luftdynamik,
bidirektionale Fluid/Festkörper-Impulsrückgabe und globale
Energie-/Impulsbilanzen fehlen weiterhin. Keine erneute teure
6-s-Simulation wurde für diesen lokalen Randdruckbaustein ausgeführt.
Unreal-5.8-Native-E2E ist für diese neue Option ebenfalls nicht
nachgewiesen.

**Offizielle Phase P2: PARTIAL, keine Vollabnahme.** Die Änderung
allein darf weder den Phasenstatus noch `ALL_COMPLETE` auf PASS
setzen. Frühere Teilmeilensteine bleiben unverändert.
