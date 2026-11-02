# P2-Teilmeilenstein: massengewichtete duale MAC-Impulsabbildung (02.11.2026)

**Abnahmestatus: nur Teilmeilenstein PASS, P2 insgesamt INCOMPLETE.**

## Tatsächlich implementierter Schritt

`include/upc/p2_two_phase_mac_momentum_remap3d.hpp` rekonstruiert
aus der nach realem VOF-Face-Fluss konservativ transportierten
dreikomponentigen zellzentrierten Impulsdichte eine gestaffelte
3D-MAC-Geschwindigkeit. Für jedes Innenface gilt bei Zellvolumen V:

`m_face = 0.5 * (rho_left + rho_right) * V`

`u_face = (rho_left*u_left + rho_right*u_right) / (rho_left + rho_right)`

Die beiden Halbmassen des MAC-Dualvolumens werden damit konsistent
gewichtet. Das neue Live-Integrationsmodul verwendet diese Abbildung
anstelle der bisherigen ungewichteten Geschwindigkeitsmittelung,
führt danach erneut die variable-Dichte-PCG-Projektion aus und
übernimmt das Ergebnis transaktional in den nächsten Fluidzustand.

**Wichtig:** Die nicht durch innere MAC-Faces repräsentierten
halben Randzellen werden **nur bilanziert**, nicht als künstliche
Randgeschwindigkeit eingespeist oder als physikalische Wandkraft
ausgegeben. Die formale Identität

`I_cell = I_MAC_interior + I_omitted_boundary_halves`

ist eine exakte **Quadraturzerlegung** des Zellimpulses, keine
physikalisch geschlossene Wand-/FSI-Impulsbilanz.

Der Live-Ledger trennt ausdrücklich:
- rein numerische Remap-Impulsänderung,
- reale Druckprojektion/Wanddruckreaktion,
- PCG-Projektionsenergie und
- offene kinetische Remap-Energiedifferenz.

## Native Nachweise (MSVC x64 Release)

Reproduktion: `scripts/p2_two_phase_momentum_build.cmd`.
Unabhängige QA mit Native-Binary-Rerun, synthetischen Fälschungen und
SHA-256: `scripts/p2_two_phase_momentum_acceptance.py`.

- **141/141 CTest-Suites PASS**; neue remap-spezifische Tests:
  **3.543 Assertions PASS** auf 4×4×4, 6×5×4, 8×6×5 und 12×8×6.
- Die separat geprüfte zellzentrierte konservative Impulsadvektion:
  **39.191 Assertions PASS**.
- Echte Live-3D-MAC/VOF/Impuls/PCG-Rückkopplung:
  **5.691 Assertions PASS** auf drei Gittern, je sechs Schritte.
- Maximaler Fehler der unabhängig aus den Zelldichten
  rekonstruierten Innenface-Geschwindigkeiten: **2,78e-17 m/s**.
  Maximaler Rest der Zell-/Innenface-/Randhalben-Impulszerlegung:
  **7,99e-15 kg m/s**.
- Positiver Jensen-Interpolationsverlust bei räumlich variierenden
  Geschwindigkeiten: **0,00328688 J** (8×6×5),
  **0,00815509 J** (12×8×6). Uniforme Geschwindigkeit bleibt
  trotz variabler Dichte erhalten.
- Die unabhängige QA verwirft **17/17** manipulierte oder
  unvollständige native Nachweise.

## Negative Ergebnisse — nicht kaschiert

| Live-Gitter | nicht geschlossene Remap-Impulsänderung | physikalischer Wanddruck-Impuls (separat) | Remap-Energiedifferenz |
|---|---:|---:|---:|
| 4×4×4 | 0,0482663 kg m/s | 0,0305656 N s | 0,00343517 J |
| 6×5×4 | 0,0490108 kg m/s | 0,0378122 N s | 0,00490735 J |
| 8×6×5 | 0,0526720 kg m/s | 0,0439545 N s | 0,00646473 J |

Diese Größen sind **keine** numerischen Fehlernullen. Die
Druckprojektion schließt ihren separaten Wanddruckimpuls auf
bis zu 9,58e-16 kg m/s und ihre separate Projektionsenergie
auf bis zu 6,55e-17 J, aber die gesamte bewegte
Wasser/Luft-/Wand-/Festkörper-Energiebilanz ist **nicht**
geschlossen. Eine Randhalbzelle ist kein dynamischer Festkörper.

## Offene P2-Vollabnahme

Es fehlen weiterhin die vollständig stresskonsistente dynamische
3D-Wasser-Luft-Grenze (Normal-/Tangentialspannung und
Oberflächenspannung mit zeitlich gekoppelter Luftströmung),
bidirektionale bewegte Festkörper-Fluid-Kopplung,
eine physikalisch geschlossene globale Impuls-/Massen-/Energiebilanz,
die echte MARIN/SPHERIC-H1–H4/P1–P8-Messabnahme über >=6 s
mit unabhängiger Dreigitterkonvergenz sowie das neue
Unreal-5.8-Native-E2E.

Die bisherigen 6-s-MARIN-Höhen-RMSE überschreiten die
Abnahmegrenzen; der neu geprüfte MAC-Remap ist **kein**
Ersatz für diese fehlende experimentelle Validierung.
Das offizielle `scripts/phase_gate.py` muss weiterhin
`P2` unter `GATES_INCOMPLETE` ausweisen.

## Belegorte

`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_two_phase_momentum_*.txt`
und `p2_two_phase_momentum_partial.json` enthalten
Native-Ausgaben, CTest, Build, Negativ-QA, offizielle Gate-Ausgabe
und SHA-256 von Quellen, Binaries und Logs. Alle Pfade verbleiben
unter `D:\Projekt Engine`; kein GitHub-Push.
