# P2 Teilnachweis: zuerst primal projizierter MAC-Praediktor

Stand: 28.10.2026. **P2 bleibt PARTIAL.**

## Abgegrenzter Implementierungsschritt
`P2MovingMacSharedPredictor3D::project_primal` projiziert jetzt
optional EIN gemeinsames gestaffeltes 3D-Geschwindigkeitsfeld gegen
die exakte geometrische Kontinuitaetsbedingung eines bewegten
achsenparallelen Cut-Cell-Quaders. Die zeitintegrierten, korrigierten
Primal-Face-Volumenfluesse werden mit den tatsaechlichen
zeitgemittelten offenen Flaechen wieder in m/s umgerechnet.
Ungueltige Dichte, Face-Dimensionen, nichtfinite, blockierte und
aeussere Normal-Faces werden verworfen. Der Aufruferzustand bleibt
unveraendert. `step_projected_primal` fuehrt die korrigierten
Geschwindigkeiten dem bestehenden Halbzellen-Rekonstruktor und den
drei unabhaengigen Dual-Projektionen zu.

## Reproduzierbare Tests
Der vorhandene MSVC-Release-Build und die komplette CTest-Suite
laufen mit `scripts/p2_moving_mac_shared_predictor_full_build.cmd`.
Im bestehenden Native-Test werden zusaetzlich 12 bewegte 3D-Cut-Cell-
Faelle (8^3/12^3, alle Achsen, beide Bewegungsrichtungen), ein
unabhaengig ueber alle Zellen berechnetes geometrisches
Divergenzresiduum, konservative Dual-Komponentenintegrale und die
identische Weitergabe der korrigierten Primal-Geschwindigkeiten
geprueft. Zwei stationaere analytische Druckgradientfelder
muessen durch die Projektion verschwinden. Ungueltige
Aussenrand-Geschwindigkeiten und Dichte werden abgewiesen.

Gate: `scripts/p2_primal_projected_shared_acceptance.py`.
Nachweise: `ENGINE TEST STUBE/runs/p2_primal_projected_shared_partial.json`
und `p2_moving_mac_shared_predictor_full_ctest_20261028.txt`.
Die Reports enthalten SHA-256 der Quellen und des CTest-Protokolls.

## Physikalische Grenze
Die Primal-Geschwindigkeit ist nun selbst druckprojiziert, aber
die drei MAC-Dualgitter werden danach **weiterhin separat**
projiziert. Das ist **kein gemeinsamer korrigierter physikalischer
Impulstensor**, keine stresskonsistente freie Oberflaeche und
keine bidirektionale Fluid-Festkoerper-Kopplung. Keine externen
3D-CFD-Messreferenzen, keine globale Impuls-/Energiebilanz,
keine Live-`CoupledMacWater3D`-Integration und kein neuer
Unreal-5.8-Native-E2E-Nachweis. P2-P8 bleiben offen.
