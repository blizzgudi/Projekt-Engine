# P2 Teilabnahme: periodische 3D-MAC/VOF-Druckarbeits- und Energiebilanz

Stand: 28.10.2026. **P2 PARTIAL, keine Vollphasenfreigabe.**

## Implementierter Umfang

Der optionale Header `include/upc/p2_periodic_mac_vof_energy3d.hpp` verbindet
die vorhandene nichtlineare MAC-Selbstadvektion, periodische PCG-Druckprojektion
und passiven VOF-Transport in einem diagnostischen Schritt. Er berechnet
kinetische Energien vor und nach Advektion sowie nach der Projektion
für eine konstante Dichte auf **einmal gezählten** periodischen
staggered-MAC-Faces (Seam-Kopien zählen nicht doppelt).

Für `delta_u = u_projected - u_predicted = -(dt/rho)*grad_h(p)` und
`V=dx^3` gilt die diskrete Identität:

`K_projected-K_predicted = dt*V*sum_cells(p*div_h(u_predicted)) + 0.5*rho*V*sum_unique_faces(delta_u^2)`.

Zusätzlich werden `rho*V*sum_unique_faces(u_projected*delta_u)`
(Orthogonalitätsresiduum), die maximale Abweichung der MAC-Druckgradientkorrektur
und der konservative VOF-Wasserfraktionswert geprüft. Alle Eingaben bleiben
bei Fehlern unverändert. Die bestehende Standard-API bleibt unverändert.

## Reproduzierbare Tests

- `scripts/p2_periodic_mac_vof_energy_full_build.cmd`: MSVC Release
  und vollständiger CTest-Lauf, protokolliert in
  `ENGINE TEST STUBE/runs/p2_periodic_mac_vof_energy_full_ctest_20261028.txt`.
- `tests/p2_periodic_mac_vof_energy3d_tests.cpp`: drei nichtkubische
  3D-Gitter (6x7x8, 8x9x10, 10x12x14), vier nichtlineare Schritte pro
  Gitter, Vergleich jedes korrigierten MAC-Faces und jeder Wasserfraktion
  mit dem bestehenden Solver, numerische Energiebilanz, CFL, Masse,
  Nichtzunahme der Upwind-/Projektionsenergie.
- Unabhängiger hergestellter Druckgradient aus periodischen
  trigonometrischen Feldern: Projektion entfernt ihn bis zur
  Rundungsgrenze. Der Druckarbeitswert entspricht der doppelten
  negativen Korrekturenergie. Absichtlich manipulierte Druckwerte,
  periodische Naht und ungültige Dichte/Wasserwerte werden zurückgewiesen.
- `scripts/p2_periodic_mac_vof_energy_acceptance.py` prüft Native-Output,
  analytische Referenzschranken, vollständiges CTest-Protokoll und
  SHA-256-Fingerprints; Ergebnis
  `ENGINE TEST STUBE/runs/p2_periodic_mac_vof_energy_partial.json`.

## Nicht nachgewiesen

Die Bilanz gilt für **konstante Dichte und vollständig periodische
3D-Domänen**. Die Wasserfraktion ist ein passiver Marker, kein
Wasser-Luft-Zweiphasenmodell. Upwind-Dissipation wird nur als
Energiedifferenz ausgewiesen, nicht in Wärme umgewandelt.
Es fehlen freie Oberflächen und Spannungsrandbedingungen, bewegte
Cut-Cell-Grenzen, Fluid-Festkörper-Gegenimpuls, globale mechanische/
thermische Energieerhaltung, unabhängige gemessene 3D-CFD-Daten,
Integration in `CoupledMacWater3D` und ein neuer UE5.8-Native-E2E-Lauf.
P2–P8 bleiben Vollphasen-offen.
