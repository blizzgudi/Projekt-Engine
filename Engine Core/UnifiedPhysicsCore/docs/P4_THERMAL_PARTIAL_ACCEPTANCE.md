# P4 — 3D Wärmeleitung und Phasenwechsel: überprüfter Teilmeilenstein

Stand: 2026-10-26. **Status: TEILABNAHME / P4 NICHT ABGESCHLOSSEN.**

## Implementiert

- Eigenständiger 3D-Finite-Volume-Wärmesolver include/upc/thermal3d.hpp für
  Zellen mit Material-IDs und SI-Einheiten. Temperatur in Kelvin, Enthalpie
  in Joule, Leitfähigkeit in W/(m K), Wärmekapazität in J/(kg K).
- Symmetrische paarweise Energieübertragung über sechs Nachbarn. Isolierte
  Außenränder, harmonische Leitfähigkeit an Materialgrenzen und explizite
  stabile Substeps. Gesamtenergie und Masse werden geprüft.
- Einphasige Festkörperzellen und reine Wasserzellen mit Enthalpiemodell
  für Eis/Flüssigkeit/Dampf. Annahme: ca. 101325 Pa, Schmelztemperatur
  273,15 K, Siedetemperatur 373,15 K. Die latente Energie wird während
  der Temperaturplateaus berücksichtigt.
- Externe Wärmequelle mit explizitem Energiekonto und 0-K-Grenzprüfung.
- Snapshot-Brücke zu UnifiedPhysicsRuntime + CoupledMacWater3D +
  VoxelGrid: die aktuellen 3D-Flüssigkeitsmassen und festen Material-
  IDs/Temperaturen werden übernommen. Die nach Wärmeleitung berechneten
  Festkörpertemperaturen können in das zentrale VoxelGrid zurückgeschrieben
  werden. **Es handelt sich ausdrücklich um einen Snapshot, nicht um
  bidirektional gekoppelte thermische Advektion.**

## Quantitative Referenzen

tests/thermal3d_tests.cpp: 841+ native Assertions, darunter
- analytische Temperaturdifferenz zweier Körper mit Fourier-Leitung,
- Wärme-/Massenbilanz über 3D-Nachbarn und Grenzflächen,
- räumliche 3D-Symmetrie,
- Schmelz- und Verdampfungsplateaus, Kondensation/Erstarrung,
- Energiezufuhr und Wärme-Rückgabe,
- thermischer Kontakt zwischen Eis und warmem Festkörper,
- Übernahme von MAC-Flüssigkeitsmasse, Voxeltemperaturen und Rückschreiben
  der Festkörpertemperatur ohne unerlaubte Änderung der Fluidmasse,
- ungültige Parameter und fehlende Gitterausrichtung.

benchmarks/p4_reference.cpp: 3D-Neumann-Wärmeleitung auf 8³, 12³,
16³, 24³ Gittern gegen die analytische zeitabhängige Eigenmode
der **räumlich diskretisierten** Wärmegleichung:

  T(x,t) = 300 K + A(t) cos(pi*(x+0.5)/N)
  A(t) = 50 K * exp(-lambda*t)
  lambda = 4 * k*dx/(m*c) * sin²(pi/(2N))

200 Zeitschritte zu 0,05 s. Ergebnis im initialen Release-Test:
max. Profilabweichung 8³: 0,00244 K;
12³: 0,000538 K; 16³: 0,000177 K; 24³: 0,0000359 K.
Energie-Residuum relativ <= 1e-15. Zeitmessung im
ENGINE TEST STUBE/runs/p4_partial_acceptance.json reproduzierbar.
Benchmark misst CPU-Leistung nur unter diesen konkreten Bedingungen.

## Noch offene P4-Akzeptanzkriterien

P4 ist erst vollständig abgeschlossen, wenn mindestens:
1. **Lebendige Kopplung** von Temperatur/Enthalpie an 3D-MAC/VOF-
   Wassertransport statt einmaligem Snapshot funktioniert.
2. **Massenerhaltender Wechsel** von Wasser zu Eis bzw. Dampf im
   gemeinsamen dynamischen Material-/Fluid-Volumen und passende
   Bewegungs-/Kollisionsbedingungen umgesetzt sind.
3. **Wärme-/Massen-/Energieerhaltung des Gesamtsystems** einschließlich
   Phasenwechsel, Strömung, Energiequellen und P3-Boden-Transfer
   in mehrdimensionalen Szenarien validiert ist.
4. Verdampfung/Kondensation und thermische Expansion realistisch genug
   für die festgelegten Szenarien berücksichtigt werden. Das jetzige
   reine Wasser-Enthalpiemodell benutzt konstante Dichte/Volumen und
   einen fixen Druck.
5. Native Regression, quantitative Referenzen, Performance-Budget und
   maschinenlesbare Quell-Hashes weiterhin erfolgreich sind.

Ein positives Ergebnis dieser Teilabnahme ist **kein P4-PASS** und
kein physikalisch vollständiger Wärme-/Fluid-/Festkörper-Simulator.

## Reproduzieren

- CMake/MSVC Release-Build im bestehenden build-Verzeichnis
- CTest physics_p4_thermal_conduction_and_phase
- CTest physics_p4_neumann_heat_reference
- C:\Python314\python.exe scripts\p4_partial_acceptance.py
- C:\Python314\python.exe scripts\phase_gate.py

Quellen der verwendeten Standardmethoden:
- Fourier-Gesetz / finite Volumenleitungsbilanz (klassisches
  konservatives Diskretisierungsschema).
- Wärmekapazitäten/Schmelz- und Verdampfungsenthalpie als in
  materials_ext.hpp bereits festgelegte reine Wasser-Konstanten.
