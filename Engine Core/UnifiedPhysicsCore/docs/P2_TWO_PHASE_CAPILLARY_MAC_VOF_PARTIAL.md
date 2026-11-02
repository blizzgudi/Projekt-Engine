# P2 Teilnachweis: kapillare Wasser-Luft-Kraft auf bewegtem 3D-MAC/VOF (02.11.2026)

**Status: PARTIAL. Kein P2-Voll-PASS.** Implementiert ist ein separat aktivierbarer
3D-Continuum-Surface-Force-(CSF-)Quellterm mit lokal aus dem aktuellen
VOF-Feld abgeleiteter Grenzflaechen-Normale und Krümmung. Der diskrete
Impulsquellterm `sigma * kappa * grad(alpha)` wirkt auf alle drei
staggered MAC-Face-Komponenten und verwendet exakt die gleichen
arithmetischen Face-Dichten wie die vorhandene variable-Dichte-Druck-PCG.
Die Druckprojektion folgt direkt nach dem Quellterm. Ein neuer optionaler
Live-Schritt verknuepft diese Projektion anschliessend mit dem vorhandenen
konservativen Zellimpuls-/MAC-Remap-/Volltensor-VOF-Schritt. Kein anderer
Solver-Pfad wird implizit umgestellt.

## Physikalische Diskretisierung und Grenzen

- Zellzentrierte 3D-Zentraldifferenzen (einseitig am Rand) fuer
  `grad(alpha)`; Normierung und `kappa = -div(n)` in 3D.
- Face-Quellbeschleunigung `dt * sigma * kappa_face *
  (alpha_right-alpha_left)/(dx*rho_face)`, danach dieselbe echte
  Neumann-PCG wie im Zweiphasen-MAC-Solver. Impermeable Waende bleiben
  undurchlaessig. Die Auswertung ist transaktional.
- Separate SI-Ledger fuer Quellimpuls, Druckwandreaktion, Face-Kinetik,
  Quellarbeit und Projektionsdissipation. Diese **Subschritt-Identitaeten**
  sind keine Gesamtenergiebilanz des transportierten Grenzflaechenareals.
- Die Grenzflaeche ist ein diffuses VOF-Zellgradientenmodell, **keine**
  geometrisch rekonstruierte PLIC-Interface-Spannung. Keine nachgewiesene
  Young-Laplace-Drucksprung-Konvergenz, keine Kontaktwinkel-/Benetzungs-
  dynamik, kein FSI und keine Oberflaechenenergiekonvergenz.
- Das explizite kapillare Zeitschrittkriterium wird als notwendige
  Stabilitaetspruefung erzwungen, nicht als hinreichende CFD-Abnahme.

## Echte Windows-MSVC-Release-Nachweise

Reproduktion:
`scripts/p2_two_phase_capillary_build.cmd` und
`scripts/p2_two_phase_capillary_acceptance.py`.
Quellen: `include/upc/p2_two_phase_capillary_mac_vof3d.hpp`,
`tests/p2_two_phase_capillary_mac_vof3d_tests.cpp`.
Protokolle/Hashes: `ENGINE TEST STUBE/runs/p2_two_phase_capillary_*.txt`
und `p2_two_phase_capillary_partial.json`.

- Planare lineare VOF-Referenz 7³: Krümmung exakt 0, keine
  kapillare Kraft, Geschwindigkeit exakt 0.
- Kugelfoermiger 3D-VOF-Tropfen Radius 0,31 m: analytische mittlere
  Krümmung 2/R = 6,451612903225807 m^-1.
  8³ mit dx=0,15 m: 5,466551847305144 m^-1;
  12³ mit dx=0,10 m: 6,272744965717555 m^-1.
  Der **Mittelwertfehler** sinkt von 0,985061 auf 0,178868 m^-1.
  Das ist keine unabhängige Young-Laplace-/Messkonvergenz.
- Echte Druckprojektion und Live-VOF-/Impuls-/Volltensor-Kopplung auf
  6³: zusätzliche Face-Geschwindigkeitswirkung 2,6979911872e-7 m/s,
  finale Divergenz 5,4806419819e-17 s^-1.
- **75 native Assertions PASS**; gezielter CTest 1/1;
  vollständiger CTest **145/145 PASS**. Unabhängige Python-QA
  wiederholt die native EXE, vergleicht die Textausgabe bytegenau,
  prueft analytische Referenz und lehnt neun gezielt manipulierte
  Beweisprotokolle ab.
- Negativtests: ungueltige/NaN-Oberflaechenspannung, ungueltige
  Mischungsdichte, zu grosser kapillarer Zeitschritt, offene
  Wandnormalgeschwindigkeit, unteriterierte Live-PCG; atomarer Rollback.

## Wichtige negative Ergebnisse

**Spurious currents verschlechtern sich in diesem Zweig mit der
Gitternachverfeinerung:** nach dem Druckschritt 5,9607262066e-7 m/s
auf 8³ und 1,0900968425e-6 m/s auf 12³. Die Krümmungs-Mittelwerte
verbessern sich, aber der statische Tropfen ist noch **nicht**
drucksprungbalanciert/gitterkonvergent. Der Bericht markiert
`spurious_velocity_grid_converged=false`.

Die vorhandenen MARIN/SPHERIC-Daten bleiben unverändert:
`p2_marin_pcg_6s_partial.json` meldet
`height_rmse_threshold_met=false` und
`pressure_experimental_validation=false`.
`p2_marin_pressure_coarse_measured_partial.json` meldet bei
allen P1-P8-Drucksensoren **keine** Grob-zu-Fein-RMSE-Verbesserung.
Diese alten kostspieligen Simulationen wurden **nicht** erneut
ausgeführt. Der neue Tropfen-Test ist kein Ersatz fuer den
>=6s-MARIN-H1-H4/P1-P8-Dreigitter-Messvergleich.

Weiter offen: echte PLIC-/Height-Function-Spannung mit
Laplace-Drucksprung, robust konvergente spurious-current-Kontrolle,
vollstaendige dynamische bidirektionale FSI, globaler Impuls/
Oberflaechenenergie/Schwerkraft-/Wandbilanzschluss und ein neuer
Unreal-5.8-Native-E2E der gesamten gekoppelten Kette.
**Offizielles `scripts/phase_gate.py`: P2=PARTIAL,
ALL_COMPLETE=False.**
