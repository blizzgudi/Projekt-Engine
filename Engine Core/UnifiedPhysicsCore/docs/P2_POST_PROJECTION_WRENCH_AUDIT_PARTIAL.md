# P2 Teilnachweis: sechsachsige MAC-Impulsbilanz nach Reprojektion

Stand: 27.10.2026. **Teilabnahme, keine vollständige P2-Freigabe.**

## Problem und Umfang

Der vorherige Schritt FrozenPressureFluidWrench3D::apply verteilt einen
entgegengesetzten Druckimpuls auf benachbarte wet-wet-MAC-Faces. Seine
sechs Komponenten schließen vor der erneuten Druckprojektion. Eine
Druckprojektion verändert aber erneut die Geschwindigkeiten und kann die
zuvor gemessene Face-Impulsbilanz zerstören. Ein vorheriger PASS darf
nicht als FSI-Gesamterhaltung interpretiert werden.

Die neue Datei include/upc/post_projection_wrench_audit3d.hpp führt
einen **rein lesenden** Vergleich dreier echter MAC-Zustände aus:

1. vor dem Gegenimpuls (nach der ersten Druckprojektion);
2. nach der angewandten sechsachsigen Gegenreaktion;
3. nach einer zweiten realen Druckprojektion.

Es werden alle internen, nicht festen Faces mit mindestens einer
benetzten Nachbarzelle erfasst. Insbesondere zählen **wet-air-Faces**
mit, die durch die Projektion verändert werden können. Die
diskreten Impulse sind rho*dx^3*(v_neu-v_alt) pro staggered Face;
die Drehimpulse r x dp, die Face-Energie 0.5*rho*dx^3*(v_neu^2-v_alt^2).
Jede Größe wird separat für Gegenreaktion, Reprojektion und gesamten
Schritt bilanziert. Außerdem werden Körper-plus-Face-Residuen und die
Divergenz nach Projektion ausgegeben. Es gibt **keine** rückwirkende
Korrektur, keine Bewegung der Körpermaske und keine Änderung der
Projektionsgleichungen.

## Nachweisaufbau

tests/p2_post_projection_wrench_audit3d_tests.cpp konstruiert
12 echte 3D-MAC-Projektionsszenarien: 8/12/16 in X, jeweils Y=X-1,
Z=X-2; geschlossene Neumann- oder offene Wasser-Luft-Grenze; Druckoffset
150 oder 1200 Pa. Ein 2x2x2-Voxelkörper wird mit dem hergestellten
Druckgradienten (7,-11,5) Pa/Zelle beaufschlagt. Der Körper hat
einen versetzten Drehpunkt. Die aus der ersten Projektion erhaltenen
Druckkräfte werden mit der vorhandenen Starrkörper-Impulsantwort
und der echten sechsachsigen Face-Gegenreaktion verarbeitet.
Eine **separat implementierte** Schleife prüft die gemessenen
Face-Impulse, Drehimpulse, Energieänderungen und Wet-Wet/Wet-Air-Zahlen
für jede der drei Zustandsdifferenzen. Negativtests prüfen Masken,
NaN und Überlauf; Nullschritt und Teleskopgleichungen werden geprüft.

Beispiel bei n=8, 150 Pa:

| Grenze | Körper+Face-Linearresiduum nach Projektion (Ns) | Körper+Face-Drehresiduum nach Projektion (Nms) |
|---|---|---|
| Wasser-Luft | (-0.026880, +0.0234773, -0.0129658) | (-0.000473848, -0.0000215346, +0.00190565) |
| Geschlossen | (-0.026880, +0.0422400, -0.0192000) | (-0.000598442, +0.0000460800, +0.00138629) |

Die zweite Projektion bringt die maximale Zell-Divergenz unter
9.31e-11 1/s, erhält aber **nicht** allgemein den sechsachsigen
Face-Impulsproxy. Im geschlossenen Fall hebt die Projektion den zuvor
auf die Fluid-Faces angewandten linearen Gegenimpuls nahezu vollständig
wieder auf. Der Körperimpuls bleibt daher als lineares Residuum stehen.
Auch das Drehmoment weist einen deutlichen **Proxy-Fehler** auf.
Dies ist eine **negative, reproduzierbare Erhaltungsdiagnose**, keine
Erfolgsmeldung für eine gekoppelte FSI-Erhaltung.

## Reproduzierbarkeit

- MSVC Release + gesamte CTest-Suite: vollständiges Protokoll unter
  ENGINE TEST STUBE/runs/p2_post_projection_audit_ctest_20261027.txt.
- Unabhängige maschinenlesbare Teilabnahme:
  scripts/p2_post_projection_audit_acceptance.py und
  ENGINE TEST STUBE/runs/p2_post_projection_audit_partial.json.
- Im JSON stehen native CSV-Ausgabe, quantitative Python-Prüfungen,
  SHA256 der getesteten Quellen, der nativen Binärdatei und des
  vollständigen CTest-Protokolls.
- Nach Änderungen an CMake werden frühere P1-Fingerprints durch
  scripts/p1_acceptance.py neu geprüft; scripts/phase_gate.py
  muss weiterhin ALL_COMPLETE=False melden.

## Nicht nachgewiesen

Keine physikalisch korrekte bewegliche Festkörper-Wandbedingung,
kein konservativer gemeinsamer Fluid-/Solid-Kontrollvolumenfluss,
keine vollständige Gesamtimpuls- oder Energieerhaltung, keine
spannungskonsistente freie Oberfläche, keine externen gemessenen
3D-Strömungsreferenzen, keine UE5.8-Native-E2E-Abnahme dieses Pfads.
P2–P8 bleiben offen. Die Diagnose macht den nächsten nötigen
Entwicklungsschritt **messbar**, ersetzt ihn aber nicht.
