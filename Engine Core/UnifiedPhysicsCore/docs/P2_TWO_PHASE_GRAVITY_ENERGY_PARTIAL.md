# P2 Teilnachweis: gravitative Energieinventur bei bewegtem Wasser/Luft-VOF (02.11.2026)

**Status: P2 PARTIAL.** Dieser Baustein ist keine globale physikalische Energieabnahme.

## Implementierung

Der reale opt-in-Schwerkraftpfad in `include/upc/p2_two_phase_mac_vof3d.hpp`
liefert jetzt eine SI-Energieinventur der bewegten 3D-Zweiphasenmischung.
Für Zellen mit VOF-Wasseranteil `alpha`, Dichte
`rho=rho_air+(rho_water-rho_air)*alpha` und Zellmitte `r` gilt
`U=-sum(rho * dx^3 * (g dot r))` in Joule.

Die Differenz `U_after-U_before` wird zusätzlich als stabile
Zell-Delta-Summe und als **unabhängige gerichtete Donor-Face-Flussidentität**
berechnet: `DeltaU_face=-(rho_water-rho_air)*dx^3*dt*
sum_faces(velocity_face*alpha_upwind*g_axis)`.
Alle drei Raumrichtungen und beide Flussvorzeichen werden berücksichtigt.
Bei konservativem VOF und undurchlässiger Box müssen Zell- und Face-Delta
bis auf Rundungsfehler übereinstimmen; sonst wirft der transaktionale
Solver einen Fehler, ohne den Zustand zu verändern.

Das vorhandene MAC-Face-Energieinventar wurde um eine zweite
Geschwindigkeits-Quadratur mit **nach VOF-Transport aktualisierter Dichte**
ergänzt. Die Differenz `K_after_density_remap-K_after_projection`
weist die mechanische Wirkung des bisher nicht gekoppelten Impulstransports
aus. `unresolved_mechanical_energy_j` ist die tatsächliche
`(K+U)_after-(K+U)_before`-Differenz in der geschlossenen Box.
**Dieser Wert wird nicht korrigiert oder als PASS auf null gesetzt.**
Er enthält fehlende Impulsadvektion, Grenzflächenarbeit und numerische
Projektionseffekte und kann daher nicht einer einzelnen Ursache zugeordnet
werden. Der bestehende reine Quell-/Druckprojektionsledger bleibt getrennt.

## Reale native Reproduktion

Befehl: `scripts/p2_two_phase_gravity_energy_build.cmd` (MSVC Release,
lokaler CMake/Ninja, ohne andere Projektverzeichnisse).
Die erste native Testversion schlug **korrekt fehl**:
`FAIL: static hydrostatic equilibrium should remain stationary`.
Ursache war eine falsche Diagnoseformel, die die hydrostatische
Projektionskorrektur zusätzlich als physikalische Dissipation verbuchte
und damit bei ruhender Flüssigkeit fälschlich 0,0274636657458 J
Restfehler ausgab. Nach Trennung der numerischen Projektionsdissipation
von der physikalischen `Delta(K+U)`-Inventur wurde der Test nativ
neu gebaut und wiederholt. Das ist eine dokumentierte negative
Testbeobachtung, kein Modellvalidierungs-PASS.

Ergebnis des letzten Native-Tests: **221 Assertions PASS**, 4 echte
3D-Rasterfälle mit je sechs Zeitschritten und 1 Rollback-Negativtest:

| Raster | Fall | Max. abs. grav. Fluss (J) | Max. abs. mechanische Energielücke (J) | Max. abs. kinetische Dichte-Remap-Differenz (J) |
|---|---|---:|---:|---:|
| 4×4×4 | hydrostatisch | 2.51e-17 | 0 | 0 |
| 6×5×4 | hydrostatisch | 1.41e-15 | 1.07e-13 | 1.79e-43 |
| 6×5×4 | geneigte 3D-Phasen, g=(1.7,-3.2,-9.81) | 0.0353656437523 | **0.0361107310582** | 0.0006429117443 |
| 8×6×5 | geneigte 3D-Phasen, g=(1.7,-3.2,-9.81) | 0.0690339892656 | **0.0667584642975** | 0.0007860340111 |

Der höchste beobachtete Zell-/Face-Flussidentitätsfehler liegt bei
1.49e-15 J. Der **nicht geschlossene** mechanische Energiehaushalt
in den bewegten Fällen ist ein negativer physikalischer Befund, kein
akzeptiertes Gesamtenergiekriterium.

Vollständiger nativer Rebuild und **138/138 CTest PASS**. Unabhängiger
Python-QA-Rerun mit Trace-Fälschungsprüfungen und SHA-256:
`scripts/p2_two_phase_gravity_energy_acceptance.py`,
`ENGINE TEST STUBE/runs/p2_two_phase_gravity_energy_partial.json`.

## Grenzen und nächste Schritte

- Der Druck-/Schwerkraft-/VOF-Pfad besitzt noch **keine konservative
  Impulsadvektion** und keine stresskonsistente bewegte Wasser/Luft-Grenze.
- Die neuen 3D-Gitter sind manufactured/diagnostisch, **keine**
  unabhängige MARIN/SPHERIC-Messkonvergenz.
- Der historische 6-s-MARIN-Abgleich verfehlt die H1–H4-Grenzen;
  P1–P8-Drucksensoren und unabhängige Dreigittermessungen sind offen.
- Dynamische bidirektionale FSI, globale physikalische Impuls-/Energie-
  abnahme und Unreal-5.8-Native-E2E für diesen Pfad sind nicht belegt.
- Der offizielle `scripts/phase_gate.py` muss **P2 PARTIAL** melden.
