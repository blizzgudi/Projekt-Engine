# P2-Teilnachweis: gemessene 3D-MAC-Geschwindigkeitsgradienten beider Phasen (02.11.2026)

## Implementiert

Der neue read-only Baustein `include/upc/p2_mac_two_phase_traction3d.hpp` liest die **tatsächlichen staggered 3D-MAC-Face-Geschwindigkeiten** und rekonstruiert an einer vorgegebenen SDF-Wasser/Luft-Schnittfläche getrennte 3x3-Geschwindigkeitsgradienten für Wasser und Luft. Eine zellzentrierte Geschwindigkeit wird aus den beiden MAC-Faces gemittelt; jede Ableitung nutzt ausschließlich gleichphasige Nachbarzellen, zentriert wenn möglich und sonst einseitig. Fehlende gleichphasige Ableitungsstencils werden abgelehnt statt über die Phasengrenze zu differenzieren.

Der diagnostische viskose Spannungssprung lautet `t=(mu_w*(G_w+G_w^T)-mu_a*(G_a+G_a^T))*n`. Der Normalanteil addiert sich zu `p_air+sigma*kappa`, der Tangentialanteil wird gegen den vorgegebenen Marangoni-Gradienten projiziert und **als Residuum ausgewiesen**. Das Residuum wird ausdrücklich **nicht künstlich auf null korrigiert**. Damit ist dies eine überprüfbare Grundlage für die spätere echte Stresskopplung, aber noch keine freie-Oberflächen-Dynamik.

## Tatsächlicher nativer Nachweis

`scripts/p2_mac_two_phase_traction_build.cmd` führt einen MSVC-x64-Release-CMake-Build, den nativen Test und die gesamte CTest-Suite aus. Getestet wurden getrennte, stetig verbundene, aber unterschiedlich geneigte affine 3D-Geschwindigkeitsfelder auf 12³ und 24³ Zellen, mit Wasser/Luft-Grenze in x-Richtung. Der unabhängige analytische Soll-Tangentialstress ist `(0, 3.295, -1.14) Pa` bei `mu_w=2 Pa*s` und `mu_air=0.5 Pa*s`.

- 12³: 144 SDF-Schnittfaces, maximaler Fehler aller 3x3-Gradienten `7.77156e-16 1/s`, maximales tangentiales Residuum `1.35064e-15 Pa`.
- 24³: 576 SDF-Schnittfaces, maximaler Gradientenfehler `1.9984e-15 1/s`, maximales tangentiales Residuum `4.86982e-15 Pa`.
- Der Normaldruck blieb in beiden Fällen bei `120 Pa` (maximaler Fehler 0 Pa). **3616 native Assertions PASS**, **131/131 CTest PASS** (47.28 s).
- Negative Tests: bewusst falscher Marangoni-Gradient erzeugt ein nichtnulliges Stressresiduum; inkonsistente MAC-Phasenmaske, einseitig nicht auflösbare Phasenableitung und nichtfiniter Oberflächengradient werden abgewiesen. Separater Python-QA-Parser und absichtlich manipulierte Native-Traces werden unabhängig geprüft.
- Artefakte und SHA256: `ENGINE TEST STUBE/runs/p2_mac_two_phase_traction_partial.json`, die zugehörigen `_native.txt`, `_build.txt`, `_ctest.txt` und `_phase_gate.txt`.

## Fehlende P2-Vollabnahme

Dies ist ein **manufactured-field-Test**, keine reale MARIN-Messkonvergenz. Die Phasenfelder und die SDF sind vorgegeben; keine zeitliche Luftadvektion, keine viskose MAC-Ghost-Bedingung, kein dynamisches Tangentialstress-Feedback in den Solver, keine VOF-zu-SDF-Validierung, keine bewegte bidirektionale FSI und keine global geschlossene Massen-/Impuls-/Energiebilanz. Die historische echte 6-s-MARIN-H1-H4-Validierung bleibt FAIL; unabhängige 6-s-Dreigitter- und aufgelöste P1-P8-Messabnahmen fehlen. UE5.8-Native-E2E für diesen neuen Pfad fehlt.

**P2 bleibt PARTIAL, phase_complete=false.** P0/P1 werden durch diese Teilmessung nicht zur P2-Vollabnahme umgedeutet.

## Dokumentierter QA-Negativlauf

Nach einer rein formalen CMake-EOF-Korrektur bestand die native CTest-Suite erneut (131/131), aber der unabhaengige QA-Lauf schlug zunaechst fehl: Der inkrementelle Build enthielt nur 'ninja: no work to do', waehrend die QA einen expliziten Compilerlauf verlangte. Der Test-Quellzeitstempel wurde ohne Inhaltsaenderung aktualisiert, der MSVC-Target-Build tatsaechlich neu ausgefuehrt und die vollstaendige CTest-Suite nochmals mit 131/131 PASS geprueft (7.27 s). Danach bestanden der unabhaengige Native-Rerun und alle sechs negativen Trace-Manipulationen. Keine physikalischen Grenzwerte wurden gelockert.
