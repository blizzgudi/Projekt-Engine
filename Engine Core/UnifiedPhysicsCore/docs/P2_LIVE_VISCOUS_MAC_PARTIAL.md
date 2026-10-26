# P2 — Opt-in Live-Viskosität im 3D-MAC/VOF-Solver (Teilnachweis)

**Status: TEILNACHWEIS, P2-VOLLPHASE WEITERHIN OFFEN.** 26.10.2026.

## Änderungen
- `include/upc/coupled_water3d.hpp` integriert den vorhandenen
  `ViscousMacDiffusion3D::step_masked` als **optionalen** Schritt in
  `CoupledMacWater3D::substep` nach der semi-Lagrange-Geschwindigkeitsadvektion,
  vor Gravitation und Druckprojektion.
- `set_kinematic_viscosity(nu_m2_s)` ist explizit opt-in; Standard
  `nu=0` lässt den bisherigen inviskosen Solver unverändert.
- Aktuelle Flüssigkeitsfüllstände liefern die temporäre Wet-Maske.
  Die bereits vorhandene Solid-Maske erzwingt stationäre Hindernisse;
  der vorhandene Operator verwendet vereinfachte Dirichlet-/Neumann-Ränder.
- `last_viscosity_substeps`, `last_viscous_energy_before/after`
  erlauben einen überprüfbaren Operator-Nachweis.
- Keine neue Material- oder Voxel-Datenbank; keine Veränderung an
  fremden Repositories.

## Reale MSVC-Release-Messung
- `tests/coupled_viscous_water3d_tests.cpp`: 12.553 native Assertions.
- 9×6×7 3D-Zellen, dx=0,1 m, stationäres inneres Hindernis,
  freie Wasseroberfläche, 16 gekoppelte Zeitschritte mit dt=0,02 s.
- Wasser-Masse 90 kg -> 90 kg, numerisch innerhalb 1e-8 kg.
- 36 interne viskose Substeps; dissipierter Face-Energie-Proxy
  0,0119468516707611 (nicht vollständige mechanische Energie).
- Maximaler VOF-Unterschied gegenüber dem inviskosen Vergleich
  0,121149145596311: Die Viskosität verändert die **live**
  Wassertransport-Ergebnisse, nicht nur einen isolierten Operator.
- `nu=0` ist für alle geprüften VOF-Zellen bitgleich zum Legacy-Pfad;
  keine Wasserzelle in festen Hindernissen, stabile Projektion.
- Großer Viskositätswert: 8 interne Substeps; ungültige Eingaben abgewiesen.
- Bisherige Legacy-`coupled_water3d_tests.cpp`: 1.918 Checks PASS.
- Neuer CTest `physics_p2_live_viscous_3d_mac_vof`.
- Reproduzierbares Teil-Gate:
  `scripts/p2_live_viscosity_acceptance.py`;
  `ENGINE TEST STUBE/runs/p2_live_viscosity_partial.json`.

## Explizite Grenzen
- Die Wasser-Luft-Grenze nutzt nur einen Nullgradient-Proxy,
  **keine** vollwertige tangentiale Spannungsrandbedingung.
- Feste Hindernisse sind stationär; kein Impulsaustausch mit
  bewegten Festkörpern, keine bidirektionale FSI.
- Keine unabhängige experimentelle 3D-CFD-Validierung.
- Energieangabe ist ein Face-Quadrat-Proxy, keine globale
  mechanische Energieerhaltung.
- Zusätzlich wurde der viskose Pfad in der isolierten UE5.8-P8-Demo
  mit nu=0,005 m²/s im echten GAME- und PIE-Prozess ausgeführt:
  beide ExitCode 0, jeweils 111 viskose Substeps, 128 kg erhalten,
  960×540 GPU-SceneCapture-PNG. Siehe
  `docs/P8_UNREAL58_NATIVE_E2E_ABNAHME.md` und
  `ENGINE TEST STUBE/runs/p8_acceptance.json`.
  Dies ist ein visueller **Native-E2E-Prototyp**, keine physikalische
  CFD-Validierung und keine GPU-Beschleunigung des Solvers.
- Alte Reports mit veralteten Quellhashes sind historische Belege.
  Die betroffenen CPU- und UE5.8-Prototyp-Abnahmeskripte wurden
  für den aktuellen Stand erneut ausgeführt.

**P2 bleibt PARTIAL** bis unabhängige 3D-Referenzen, dynamische
Solid-Grenzen, Impuls-/Energie-/Massen- und Laufzeitnachweise vollständig
vorliegen. P3–P8 sind dadurch ebenfalls nicht automatisch fertig.
