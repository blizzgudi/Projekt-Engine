# P2 Teilabnahme: projizierter dynamischer Quader mit konservativem Wasser-Fraction-Remap (29.10.2026)

**Status: PARTIAL. P2 ist NICHT vollständig abgenommen.**

## Implementierung

`P2MovingBoxProjectedVof3D` verbindet den bestehenden dynamischen
Quader-Druckimpuls, seine auf MAC-Faces angewendete Gegenreaktion und die
gemeinsame Cut-Cell-Druckprojektion mit dem vorhandenen konservativen
`P2MovingCutCellScalarTransport3D`. Der projizierte **zeitintegrierte
Volumenfluss** transportiert eine Wasser-Füllfraktion pro **offenem**
Fluidvolumen. Für jede Zelle gilt:

`alpha_new * f_new = alpha_old * f_old - div(q * f_upwind) / dx^3`.

Der neue Pfad verwendet exakt dieselben projizierten Fluxes wie die
zurückgegebenen MAC-Geschwindigkeiten, ohne die Druckprojektion oder
den produktiven `CoupledMacWater3D` zu verändern. Donor-CFL,
geometrische Kontinuität, 0..1-Beschränkung, Zellvolumen und
Wassermasse werden überprüft. Ungültige Eingaben werden abgelehnt.

## Reproduzierbare Prüfungen

- MSVC Release: `scripts/p2_moving_box_projected_vof_full_build.cmd`.
- Native Tests: `tests/p2_moving_box_projected_vof3d_tests.cpp`.
- 24 Einzelschritt-Konfigurationen: 8³/12³, alle drei Achsen, beide
  Druckgradient-Vorzeichen, räumlich variable und vollständig gefüllte
  Wasserfraktionen. Eine **unabhängige** Face-für-Face-Neuberechnung
  prüft jede transportierte Zellfraktion und die GCL.
- 12 zusätzliche **zweischrittige** Fälle verwenden die neue
  Wasserfraktion, Quaderposition, Quadergeschwindigkeit und projizierte
  MAC-Geschwindigkeit aus dem vorigen Schritt.
- Negativtests: NaN, Überfüllung, falsche Shape, Wasser in komplett
  blockierter Zelle und ungültige Dichte.
- Erwartete Grenzen: 81/81 CTest-Suites, mindestens 67.000 native
  Assertions, 24 VOF- und 12 sequenzielle Referenzzeilen.
  Verbindliche tatsächliche Resultate und Source-SHA256 stehen in
  `ENGINE TEST STUBE/runs/p2_moving_box_projected_vof_partial.json`;
  vollständiges CTest-Protokoll im gleichen runs-Ordner.

## Wichtige fachliche Grenzen

Das ist **passiver** konservativer VOF-Transport auf einem bereits
berechneten inkompressiblen Geschwindigkeitsfeld, keine freie
Oberfläche mit korrekt aufgelösten Oberflächenspannungen oder
Wasser-Luft-Spannungsrandbedingungen. Der Druck ist weiterhin aus
einem **vorgegebenen eingefrorenen** Feld, nicht aus der Wasserfraktion
und einer iterativen Fluid-Festkörper-Kopplung abgeleitet.

Der bereits nachgewiesene große Projektions-Impulsproxy (rund 350 Ns)
bleibt **ungeschlossen**. Weder physikalische globale Impuls- und
Energieerhaltung noch eine echte Wasser-Luft-/Boden-/Wärme-/Festkörper-
Gesamtkopplung, externe 3D-CFD-Messvalidierung oder Unreal-5.8-Native-E2E
für diesen neuen Pfad werden behauptet.

**Vollphasenstatus:** P0/P1 PASS; P2-P8 PARTIAL;
`ALL_COMPLETE=False`.
