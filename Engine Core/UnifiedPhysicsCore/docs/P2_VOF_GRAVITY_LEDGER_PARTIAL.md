# P2 Teilabnahme: diskrete 3D-VOF-Gravitationspotenzialbilanz (31.10.2026)

**Status:** Teilnachweis, nicht P2-Vollabnahme. P2–P8 bleiben INCOMPLETE.

## Implementierung

Der vorhandene 3D-MAC/VOF-Solver CoupledMacWater3D liefert nun eine
read-only-Zellmittelpunktdiagnostik water_gravitational_potential_j() mit
z-Achse nach oben und Referenzhöhe z=0. Im konservativen VOF-Transport
akkumuliert last_vof_transport_potential_delta_j() die tatsächlichen
Volumenverschiebungen über Zellgrenzen und alle adaptiven Substeps.
Für jeden Transfer mit Wasserfüllungsanteil Δf und Zellen a→b gilt:

- Δm = ρ Δx³ Δf
- ΔE_pot = Δm · 9,81 m/s² · (z_b-z_a) Δx

Horizontale Transfers haben damit exakt keinen Gravitationsbeitrag.
Die Diagnostik ist transaktional im opt-in-Pfad
step_verified_reprojected: Bei Unterkonvergenz bleiben VOF, Potenzial,
Ledger und verzögerte Transportereignisse unverändert.

## Reproduzierbare Prüfung

- scripts/p2_vof_gravity_ledger_full_build.cmd baut mit MSVC Release
  und führt die komplette CTest-Suite sowie den neuen Native-Test aus.
- tests/p2_vof_gravity_ledger3d_tests.cpp prüft einen analytischen
  statischen Zwei-Zellen-Fall, unabhängige Anfangspotenziale,
  die Massenerhaltung, den Potenzialunterschied zwischen Anfangs- und
  End-VOF und eine separat aus tatsächlichen Transportereignissen
  berechnete Potenzialänderung; außerdem Unterkonvergenz-Rollbacks.
- scripts/p2_vof_gravity_ledger_acceptance.py prüft unabhängig
  8³- und 12³-Gitter, neun Quell-SHA256, Native-Binärhash und CTest-Loghash.
- Nachweise: ENGINE TEST STUBE/runs/p2_vof_gravity_ledger_partial.json,
  p2_vof_gravity_ledger_ctest_20261031.txt und
  p2_vof_gravity_ledger_build_20261031.txt.

Messung: 7 kg Wasser, 19 Transportereignisse je Gitter;
8³: 33,8445 J → 33,726055569230788 J;
12³: 47,5785 J → 47,460055569230796 J.
Beide liefern ΔE_pot = -0,11844443076921887 J; die
unabhängige Transportereignis-Summe ist identisch. Die finalen
MAC-Wasserzellen-Divergenzen liegen bei etwa 1,30e-11 s^-1.
92/92 CTest-Suites und 72 native Assertions PASS.

## Grenzen

Dies ist eine diskrete potenzielle Energie des transportierten
VOF-Masseninventars, keine physikalische Gesamtenergiebilanz.
Kinetische Energie, Druckarbeit, Viskositätswärme, Wasser-Luft-
Grenzflächenspannung, bewegte Festkörper und globale FSI-Impuls-/
Energieerhaltung sind nicht enthalten. Insbesondere ist die
semi-Lagrange-Geschwindigkeitsadvektion dissipativ. Keine externe
3D-CFD-Messvalidierung oder neue Unreal-5.8-Native-E2E-Abnahme.
P2 bleibt offen.
