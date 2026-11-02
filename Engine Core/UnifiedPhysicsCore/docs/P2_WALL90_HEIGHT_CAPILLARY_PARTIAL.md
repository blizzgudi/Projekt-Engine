# P2 – 90°-Wandkontakt für 3D-VOF-Höhenfunktionen (Teilnachweis, 02.11.2026)

**Status: PARTIAL. Kein P2-Voll-PASS, kein experimenteller
MARIN/SPHERIC-Nachweis.** Eigenständiges Projekt
`D:\Projekt Engine\Engine Core\UnifiedPhysicsCore`.

## Reproduzierter Defekt

Die bisherige streng einwertige 3D-VOF-Höhenfunktion berechnete
die Krümmung in den tangentialen Randzellen durch Übernahme der
Krümmung aus der ersten inneren Spalte. Bei einem glatten, mit
90°-Kontaktwinkel an seitliche Wände anschließenden
3D-Wassergraphen wurde dadurch die Randkrümmung stark verzerrt.
Der erste native Build des neuen Tests schlug erwartungsgemäß
wegen fehlender Opt-in-API fehl. Diese Diagnose wurde im Werkzeuglauf
beobachtet; der temporäre Buildlog wurde beim erfolgreichen Build
überschrieben und ist **nicht** als eigener Artefaktnachweis vorhanden.
Ein zusätzlicher Build ohne MSVC-Entwicklungsumgebung schlug wegen
fehlender Standard-Include-Pfade fehl. Dieser **Umgebungsfehler**
ist separat unter `p2_wall90_initial_expected_failure_20261102.txt`
gespeichert und wird nicht als physikalischer Defekt ausgegeben.

## Änderung

- `include/upc/p2_vof_height_curvature_field3d.hpp`:
  explizites opt-in `wall_neumann_90=true`, gespiegelt
  cell-centered ghost heights `h[-1]=h[0]`, `h[N]=h[N-1]`.
  Diese Diskretisierung setzt den Normalgradienten an einer
  geschlossenen seitlichen Wand auf null; bei einer graphförmigen
  Flüssig-Luft-Grenze entspricht dies einem vorgeschriebenen
  **statischen Kontaktwinkel von 90°**. Die alten
  Randextrapolationswerte bleiben Standard.
- `include/upc/p2_two_phase_capillary_mac_vof3d.hpp`:
  das neue opt-in wird über den echten Kapillar-MAC/PCG-Operator
  und den bewegten 3D-Impuls-/Viskositäts-/VOF-Schritt geführt.
  Ungültiger Wandmodus ohne Höhenfunktion wird abgelehnt.
  Die Opt-in-Nutzung erscheint separat im Ergebnisledger.
- `tests/p2_wall90_height_capillary3d_tests.cpp`:
  unabhängige analytische Cosinus-Oberfläche mit
  90°-Wandkontakt, drei Achsen, drei Gitter, statische
  PCG-Druckprojektion, bewegte 3D-VOF-Phase,
  Massenerhaltung und negative Transaktionsprüfungen.

## Echte quantitative Ergebnisse

Die analytische Referenz benutzt
`h(u,v)=0.6+0.02*cos(2*pi*u/1.2)+0.015*cos(2*pi*v/1.2)`
mit analytisch verschwindendem Wandnormalgradienten.
Die drei Raumachsen liefern dieselben unabhängigen
Eckkrümmungsfehler:

| Gitter | neuer absoluter Fehler (1/m) | alter extrapolierter Fehler (1/m) |
|---|---:|---:|
| 12³ | 0.02086476885 | 0.26786309619 |
| 20³ | 0.00775281633 | 0.10231801070 |
| 32³ | 0.00306144770 | 0.04081174375 |

Die Fehler nehmen mit Verfeinerung ab. Der Opt-in-Wandmodus
ändert die real projizierten MAC-Geschwindigkeiten um maximal
`7.2867695149e-8 m/s`; die statische Druckprojektion
hat `5.7688025906e-17 1/s` Divergenz. Im bewegten
3D-MAC/VOF-Fall ist die letzte Divergenz
`7.0239360118e-17 1/s`, Wasser-Massenrest
`3.4106051316e-13 kg`. Der Nativtest meldet
**66 Assertions PASS**; vollständiger Release-Regressionslauf
**150/150 CTest PASS**. Der QA-Bericht enthält
SHA-256-Fingerprints von Quellen, Binary und Rohlogs,
einen unabhängigen nativen Rerun und absichtlich manipulierte
Protokolle, die die QA ablehnen muss.

## Grenzen / negative Befunde

Der neue Modus ist ein **vorgegebener statischer 90°-Wandwinkel**,
keine dynamische Kontaktwinkel-/Wetting-Physik.
Das Graphmodell lehnt Überhänge und Mehrfachschnitte ab;
es ist keine allgemeine 3D-Tropfen-PLIC-Rekonstruktion.
Es beweist keine konvergierende Young-Laplace-Kapillarströmung
auf bewegten Gittern, keinen physikalisch geschlossenen
Oberflächenenergie-/Fluidimpulshaushalt, keine
bidirektionale bewegte FSI, keinen vollständigen
MARIN/SPHERIC-H1–H4/P1–P8-Vergleich über mindestens
6 Sekunden auf drei Gittern und kein neues UE5.8-Native-E2E.
Der frühere negative Verfeinerungsbefund der
Kapillar-Spurious-Currents bleibt **unverändert offen**.

## Reproduktion

1. `scripts/p2_wall90_height_build.cmd` (MSVC-vcvars64,
   Target-Build, Native-Test, gezielter CTest).
2. `scripts/p2_wall90_full_regression.cmd` (gesamter
   MSVC-Release-Build und vollständiger CTest).
3. `scripts/phase_gate.py` (offizieller Phasenstatus).
4. `scripts/p2_wall90_height_acceptance.py`
   (unabhängige analytische QA, Native-Rerun, negative
   Manipulationsprüfungen und SHA-Bericht).

Alle Nachweise ausschließlich unter
`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_wall90_*`.
Die zwei schon vorher lokal geänderten P2-Reports bleiben
unberührt und werden nicht mitcommittet.
