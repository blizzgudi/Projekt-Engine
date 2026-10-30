# P2 Teilabnahme: nichtlineare Druck-Wrench-ODE mit 6-DOF-MAC-Gegenimpuls

Stand: 30.10.2026. **Nur ein P2-Teilmeilenstein, kein P2-Vollabschluss.**

## Zweck

Der bisherige Adapter P2FrozenWrenchRigidStep3D berechnet aus einer
vorgeschriebenen, achsenparallelen fraktionalen Druckbox die resultierende
Kraft und das Drehmoment um die Koerpermitte. Die Starrkoerper-ODE aendert
Translation und Winkelgeschwindigkeit; die entgegengesetzten Fluidimpulse
waren bislang nur diagnostisch.

Der opt-in Adapter P2FrozenWrenchMacReaction3D uebertraegt nun den
**linearen und angularen** negativen Starrkoerperimpuls tatsaechlich auf
nahegelegene nasse MAC-Innenflaechen eines **statischen** Voxelkoerpers.
Er benutzt eine 6x6-Nebenbedingungsprojektion, welche unter allen
ausgewaehlten Face-Impulsen die quadratische Summe der
Geschwindigkeitsinkremente minimiert:

- J = sum_i m_i * dv_i * e_axis
- L = sum_i r_i x (m_i * dv_i * e_axis)
- min 1/2 sum_i m_i * dv_i^2 bei vorgegebenem (J,L)
- m_i = rho * dx^3 (nur numerischer staggered-Face-Massenproxy)
- G = sum_i m_i * a_i * a_i^T, lambda = G^(-1) * (J,L),
  dv_i = a_i^T * lambda.

Nur Faces zwischen zwei nassen, nichtfesten Zellen in direkter Naehe
des 6-Nachbar-Koerperrandes werden veraendert. Aeussere, trockene und
festkoerpernormale Faces bleiben unveraendert. Die 6x6-Loesung wird
diagonal skaliert und bei Rangdefizienz abgelehnt. Die gesamte Aenderung
wird erst nach vollstaendiger Vorpruefung geschrieben.

## Reproduzierbarer Test

- Native C++17: tests/p2_frozen_wrench_mac_reaction3d_tests.cpp
- Reproduzierbarer MSVC-Release-Build mit voller CTest-Regression:
  scripts/p2_frozen_wrench_mac_reaction_full_build.cmd
- Unabhaengige Python-Referenz und SHA256-Teilgate:
  scripts/p2_frozen_wrench_mac_reaction_acceptance.py
- Maschinenlesbarer Nachweis:
  ENGINE TEST STUBE/runs/p2_frozen_wrench_mac_reaction_partial.json
- Vollstaendiges CTest-Protokoll:
  ENGINE TEST STUBE/runs/p2_frozen_wrench_mac_reaction_full_ctest_20261030.txt

Die nativen Tests pruefen zwei 3D-Gitter (8^3/12^3), beide
Boxverschiebungsrichtungen und jeweils ruhende/initial bewegte
MAC-Faces, also acht Faelle. Eine nichtlineare Druckverteilung
erzeugt eine echte Drehmomentlast. Die Gegenreaktion wird unabhaengig
aus den veraenderten MAC-Faces inklusive Hebelarmen erneut aufsummiert.
Die Python-Referenz rekonstruiert Auswahl, Geometrie, 6x6-Loesung,
Impulse, Face-Energieproxy und Divergenz ohne den C++-Solver.
Dazu kommen deterministischer Replay, Ausschluss nichtselektierter
Faces und atomare Ablehnung fehlerhafter/unterbestimmter Eingaben.

## Aussagegrenzen und offene Vollkriterien

Die exakt gegensaetzlichen Impulse gelten **nur fuer die gewaehlte
diskrete MAC-Face-Massenbuchung vor einer Druckprojektion**. Ein
nachfolgender Inkompressibilitaetsschritt kann diese Bilanz veraendern.
Die nichtverschwindende Divergenz wird gemessen, nicht korrigiert.
Die Starrkoerpermaske bleibt stationaer, obwohl die ODE Bewegung und
Winkelgeschwindigkeit berechnet. Weder Orientierung noch
Kollisionsgeometrie werden fortgeschrieben; Druck und Traktion sind
waehrend des Zeitschritts eingefroren.

Es gibt hier **keine** global physikalisch abgeschlossene
Navier-Stokes-/Fluid-Festkoerper-Impuls- oder Energiebilanz, keine
spannungsrichtige Wasser-Luft-Grenze, keine unabhaengigen
3D-CFD-Messdaten und keine Unreal-5.8-Native-E2E-Abnahme dieses Pfads.
Die gekoppelten Wasser/Boden-, Waerme-, Festkoerper-, Reaktions- und
GPU/LOD-Vollphasen bleiben offen. P2-P8 sind INCOMPLETE;
ALL_COMPLETE=False. Ein gruenes Teilgate darf diese Werte nicht aendern.


## Nachtrag 30.10.2026: echte Nachprojektion der 6-DOF-Face-Reaktion

Die acht bisherigen 8^3-/12^3-Szenarien (beide Bewegungsrichtungen,
ruhendes/vorbelegtes MAC-Feld) werden nun nach dem eingefrorenen
6-DOF-Gegenimpuls zusätzlich mit `MacProjection3D::project(dt,3000)`
nachprojiziert. Die Tests rekonstruieren die Divergenz unabhängig aus
allen sechs Zell-Faces, integrieren lineare und angulare Druckprojektions-
Korrekturimpulse um die alte Körpermitte und prüfen die diskrete
Impulsidentität `J_total = J_frozen + J_projection` sowie endliche
Face-Energieänderung. Der ursprüngliche Reaktionspfad bleibt unverändert.

MSVC-Release: 87/87 CTest-Suites PASS, 64.310 native Assertions im
betroffenen Test. Nach Projektion liegt die maximale Divergenz in den
Szenarien in der Größenordnung 1e-10 1/s; sie sinkt jeweils um mindestens
Faktor 100. Bei ruhendem Fluid kompensiert die Projektion die ursprünglichen
Face-Linearimpulse nahezu vollständig. Bei vorbelegtem Fluid sind die
Projektionsimpulse erheblich größer, weil auch der anfängliche, an den
Außenwänden nicht kompatible MAC-Zustand korrigiert wird. Diese
Unterscheidung ist wesentlich: **Die vor der Projektion geschlossene
6-DOF-Face-Impulsbilanz ist nach der Projektion keine globale
Fluid-Festkörper-Impulserhaltung.** Es handelt sich um einen
numerischen Befund, nicht um einen physikalischen FSI-Abschluss.

Quantitative Zusatzgates und SHA-256-Nachweise stehen im bestehenden
`p2_frozen_wrench_mac_reaction_partial.json`. P2-P8 bleiben offen:
Bewegungs-/Rotationsgeometrie, physikalische Druckarbeit und Rückwirkung,
extern gemessene CFD-Referenzen und Unreal-5.8-Native-E2E fehlen.
