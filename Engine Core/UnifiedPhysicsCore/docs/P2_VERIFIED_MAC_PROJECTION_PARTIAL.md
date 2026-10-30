# P2 Teilabnahme: atomare, residualgepruefte 3D-MAC-Druckprojektion (31.10.2026)

**Status: PARTIAL, nicht P2-PASS.** Dies ist eine Sicherheits- und
Konvergenzverbesserung der vorhandenen volumetrischen MAC-Projektion,
keine vollstaendige Fluid-Festkoerper-Kopplung.

## Implementierung

In `include/upc/mac3d.hpp` gibt es jetzt die opt-in-Methode
`MacProjection3D::project_verified(dt, iterations, max_divergence_s_inv)`.
Sie fuehrt `project()` auf einer vollstaendigen lokalen Kopie aus, prueft
die Endzustaende auf endliche Druck- und Face-Werte, berechnet fuer **jede**
nasse Zelle die tatsaechliche dreidimensionale Divergenz und veroeffentlicht
den Zustand nur bei Erreichen der geforderten Toleranz. Im Fehlerfall
bleiben Druck und alle drei Face-Geschwindigkeitsfelder bitgenau erhalten.
Die alte Methode `project()` bleibt unveraendert und ungesichert;
bestehende Solver nutzen die neue Methode nicht automatisch.

Das ist wichtig, weil das bestehende Gauss-Seidel-Abbruchkriterium
eine **Druckaenderung** prueft, aber nicht direkt die physikalisch
relevante Kontinuitaetsresiduale. Eine zu kleine Iterationszahl darf
keinen teilweise projizierten MAC-Zustand publizieren.

## Unabhaengige diskrete Referenz

Zwei benachbarte nasse Zellen in einem ansonsten trockenen 3D-Gitter
mit anfaenglich nur einer inneren x-Face-Geschwindigkeit 0.12 m/s.
Die fuenf freien Nachbarseiten je Zelle haben Ghost-Fluid-Faktor 2.
Die 2x2-Poisson-Gleichung lautet

`11*p_L-p_R=-R; -p_L+11*p_R=R`,
`R=rho*dx*0.12/dt`.

Analytisch folgt `p_L=-R/12`, `p_R=R/12` und die
innere Face-Geschwindigkeit nach Projektion `0.10 m/s`.
Fuer ein isoliertes Wasser-Voxel gilt dieselbe Druckreferenz
und **eine** Gauss-Seidel-Iteration ist ausreichend.

Die nativen Tests pruefen auf 8^3 (dx=0.1 m, rho=1000 kg/m3,
dt=0.02 s) und 12^3 (dx=0.08 m, rho=1100 kg/m3,
dt=0.015 s) jeweils: 1 Iteration muss im Zwei-Zell-Fall
abgelehnt werden; Druck und saemtliche 3D-Faces bleiben unveraendert,
auch eine vorab bewegte aeussere Wand. Null-/NaN-Toleranz und
negatives dt werden ohne Mutation abgelehnt. Mit 4000 erlaubten
Iterationen werden die analytischen Druckwerte und 0.10 m/s erreicht.
Der Ein-Zellen-Fall mit einer Iteration wird dagegen akzeptiert.

## Nachweis und Reproduktion

- Native C++17: `tests/p2_verified_mac_projection3d_tests.cpp`
- MSVC-Release und gesamte CTest-Suite: 89/89 PASS
- Native Assertions: 38360 PASS
- Gemessene Zwei-Zell-Divergenz: 3.16e-14 und 3.93e-14 1/s
- Gemessene Druecke: (-50,+50) Pa bzw. (-58.6666666667,+58.6666666667) Pa
- Reproduktionsskript: `scripts/p2_verified_mac_projection_full_build.cmd`
- Unabhaengige Python-Referenz, Source-/Binary-/CTest-Hashes:
  `scripts/p2_verified_mac_projection_acceptance.py`
- JSON: `ENGINE TEST STUBE/runs/p2_verified_mac_projection_partial.json`
- Vollstaendiger CTest-Log:
  `ENGINE TEST STUBE/runs/p2_verified_mac_ctest_20261031.txt`

## Offene Vollphasen-Kriterien

Das neue Verfahren verhindert unterkonvergierte Writebacks **nur wenn
der Aufrufer die opt-in-Methode verwendet**. Es ersetzt weder eine
stabile robuste Poisson-Methode fuer alle Randbedingungen noch eine
physikalische FSI-Kraft-/Impuls-/Energie-Bilanz. Bewegliche rotierende
Geometrie, echte Wasser-Boden-Wechselwirkung in dynamischer 3D-FSI,
unabhaengige gemessene CFD-Referenzen und ein neues UE5.8-Native-E2E
fuer diesen Pfad sind nicht nachgewiesen. P2-P8 bleiben INCOMPLETE.
