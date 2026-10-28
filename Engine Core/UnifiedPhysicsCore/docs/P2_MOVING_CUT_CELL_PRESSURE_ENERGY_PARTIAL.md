# P2 Teilabnahme: Druckarbeit und kinetische Face-Energie an bewegten 3D-Cut-Cells

Stand: 28.10.2026. **P2 PARTIAL – keine vollständige CFD-/FSI-Abnahme.**

## Ziel und Implementierung

Der neue optionale Header `include/upc/p2_moving_cut_cell_energy3d.hpp`
prüft die Druckprojektion für einen vorgeschrieben linear verschobenen,
achsparallelen Starrkörperquader in einer geschlossenen, konstantdichten
3D-MAC-Domäne. Er verwendet die vorhandenen zeitgemittelten
MAC-Flächenöffnungen, die geometrischen Flüssigkeitsvolumina, die
projizierten Volumenflüsse und den Druck aus
`P2MovingBoxCutCellProjection3DBuilder`. Keine vorhandene Solver-API
wird verändert.

Für jede **einmal gezählte innere** MAC-Fläche mit Öffnungsanteil `a`:

- `q = a*dt*dx²*u` ist das während des Schritts transportierte Volumen.
- `du = dt/(rho*dx)*(p_links-p_rechts)` ist die Druckkorrektur.
- `K = 0.5*rho*dx³*sum(a*u²)` ist ein diskreter
  **zeitgemittelter Face-Energie-Proxy**, keine exakte physikalische
  Zellkinetik.
- `W_geom = dx³*sum(p*(alpha_alt-alpha_neu))` ist die diskrete
  Druckarbeit des bewegten geometrischen Volumens.
- `C = 0.5*rho*dx³*sum(a*du²)` ist die positive Korrekturenergie.

Die zu prüfende diskrete Identität lautet:

`K_nach-K_vor = W_geom-C`.

Zusätzlich werden die äquivalente Prädiktor-Form
`K_nach-K_vor = sum(q_präd*(p_links-p_rechts))+C`,
die Integration durch Teile
`sum(q_nach*(p_links-p_rechts)) = W_geom`,
die lokale GCL/Volumenkontinuität und der MAC-Druckgradient unabhängig
nachgerechnet. Nullfluss an allen sechs Außengrenzen, geschlossene
Flächen, Formfehler, nichtfinite Werte und manipulierte Druck-/Fluss-/
Geometrie-Daten werden abgelehnt.

`P2AuditedPrescribedBoxMacVof3D` ist ein opt-in Wrapper für den
bestehenden sequentiellen, vorgeschriebenen MAC/VOF-Operator. Er führt
Projektion, VOF und Energieprüfung zunächst auf einer **Zustandskopie**
aus und übernimmt den neuen Zustand nur bei vollständigem Erfolg.
Die zusätzliche Kopie ist bewusst nicht für Performance-Produktion
gedacht.

## Reproduzierbare Nachweise

- `scripts/p2_moving_cut_cell_energy_full_build.cmd`:
  MSVC-CMake-Release-Build und vollständige CTest-Regression mit
  paralleler Ausführung, Protokoll in
  `ENGINE TEST STUBE/runs/p2_moving_cut_cell_energy_full_ctest_20261028.txt`.
- `tests/p2_moving_cut_cell_energy3d_tests.cpp`: 8³, 12³ und 16³,
  drei Bewegungsachsen, beide Bewegungsrichtungen, Nullprädiktor und
  unabhängig hergestellter MAC-Gradient plus diskrete Zirkulation.
  Für den Nullprädiktor gilt analytisch `K_vor=0`,
  `K_nach=C` und `W_geom=2*C`; für den hergestellten
  Prädiktor gilt `K_vor=56 J`. Weitere Prüfungen:
  Druck-Gauge-Invarianz, zwei sequentielle MAC/VOF-Schritte mit
  Wasservolumen `n³-3.375 m³`, Fehler-Rollback sowie
  absichtlich manipulierte Druck-/Fluss-/Geometriedaten.
- `scripts/p2_moving_cut_cell_energy_acceptance.py` prüft die
  unabhängigen numerischen Referenzen, native Assertions, die
  vollständige Regression und SHA-256-Quellen-/Protokollnachweise.
  Ergebnis: `ENGINE TEST STUBE/runs/p2_moving_cut_cell_energy_partial.json`.

## Physikalische Grenzen und nächste Arbeiten

Die Bilanz ist eine **exakte Identität des gewählten diskreten
Druckprojektionsoperators**. Sie beweist weder eine korrekte
physikalische Wandspannungsarbeit noch die globale Energieerhaltung
einer Flüssigkeit mit bewegtem Festkörper. Insbesondere fehlen
konservative Impulsadvektion in Cut-Cells, dynamische Festkörper-
Gegenreaktion, freie Wasser-Luft-Oberflächen, variable Dichte,
viskose/thermische Dissipation, unabhängige gemessene 3D-CFD-Referenzen
und die Integration in `CoupledMacWater3D`. Auch UE5.8-Native-E2E
für diesen neuen Pfad wurde nicht durchgeführt. P2–P8 bleiben PARTIAL.
