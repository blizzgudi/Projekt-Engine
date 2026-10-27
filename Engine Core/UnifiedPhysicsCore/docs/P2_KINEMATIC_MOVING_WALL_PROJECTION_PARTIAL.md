# P2 Teilnachweis: vorgeschriebene 3D-Starrkörper-Normalgeschwindigkeit an Voxelwänden

Stand: 27.10.2026. **P2 bleibt offen. Dies ist ein begrenzter numerischer Teilmeilenstein.**

## Physikalische Aufgabe

Der vorherige Frozen-Pressure-Gegenimpuls wurde nach der zweiten Druckprojektion
teilweise wieder entfernt. Eine reine Nachkorrektur der MAC-Face-Impulse löst
das Problem nicht: Bei einer inkompressiblen Flüssigkeit mit undurchlässigen
Außenwänden muss die Bewegung einer inneren Festkörperoberfläche bereits als
**kinematische Randbedingung** in der Druckprojektion stehen.

Der neue unabhängige C++17-Operator
`include/upc/kinematic_wall_projection3d.hpp` setzt an jeder benetzten
Solid-Face-Normalen die vorgeschriebene Starrkörpergeschwindigkeit

`v_wall(r) = v_body + omega x (r - centre)`.

Er erhält diese Normalgeschwindigkeit bei der Poisson-Druckprojektion
(Neumann an festen Wänden, Druck 0 in Luft mit Ghost-Faktor 2),
während die verbleibenden Flüssigkeits-Faces die Zell-Divergenz reduzieren.
Außenwände bleiben undurchlässig. Die Eingaben werden vor der Veränderung
des MAC-Feldes geprüft. Ein vollständig geschlossener Fall mit nicht
kompatiblem Volumenfluss wird abgewiesen. Die Körper-Voxelmaske ist während
des gesamten Schritts **eingefroren**. Das ist eine kinematisch bewegte Wand,
**kein** zeitlich fortbewegtes, remapptes Festkörpervolumen.

**API-Grenze:** Der neue Operator liefert sein Druckfeld ausschließlich in
`KinematicWallProjectionResult3D::pressure_pa` zurück. Die interne
`MacProjection3D::pressure_pa()`-Ablage wird durch diesen separaten
Operator **nicht** aktualisiert. Eine spätere Drucktraktions-/FSI-
Kopplung darf deshalb nicht versehentlich den alten MAC-Druck lesen.

Der Operator ist ein separater experimenteller P2-Pfad; er verändert
`MacProjection3D::project` und den bisherigen `CoupledMacWater3D`-Solver
nicht. Bestehende Prototyp-Nachweise werden daher nicht nachträglich als
FSI-Abnahme umgedeutet.

## Native Prüfszenarien und unabhängige Kontrolle

`tests/p2_kinematic_wall_projection3d_tests.cpp` führt zwölf
3D-Szenarien mit Auflösungen 8×7×6, 12×11×10 und 16×15×14 aus:
jeweils geschlossener Wasserraum oder Wasser-Luft-Grenze, und
Translation oder kombinierte Translation/Rotation eines zentralen
2×2×2-Voxelkörpers. Alle zwölf Szenarien werden außerdem mit
umgekehrter Starrkörperbewegung wiederholt.

Der Test überprüft unabhängig aus der Starrkörper-Kreuzproduktformel
die 24 vorgeschriebenen Normal-Faces (8 pro Achse), die antisymmetrische
Druck- und Geschwindigkeitsantwort, unveränderte Außenwände,
null Netto-Volumenquelle, den sechsseitigen Zell-Divergenzoperator,
sowie ungültige Masken und nichtfinite Eingaben. Die vollständige
Testausgabe enthält Druck, Divergenz, Wandfluss, Iterationen und
einen Face-Energie-Proxy für jede Konfiguration.

Eine unabhängige Python-Abnahme berechnet die Wandnormalgeschwindigkeiten
und deren analytische kinetische Mindestenergie ohne Aufruf der C++-Routine.
Sie kontrolliert die nativen Ergebnisse, Quell-/Binär-Hashes und das
vollständige CTest-Protokoll. Alle Belege werden im bestehenden
`ENGINE TEST STUBE/runs`-Verzeichnis gespeichert.

## Grenzen

- **Keine Körperbewegung der Voxelmaske**, keine geometrische Remap-,
  Advektions- oder Kontakterhaltung.
- **Keine dynamische Rückwirkung** der Drucktraktion auf die
  Starrkörpergeschwindigkeit und kein gemeinsamer Impuls-/Energiesolver.
- Keine viskose tangentiale No-Slip-Bedingung, keine stresskonsistente
  freie Oberfläche, keine externe gemessene 3D-CFD-Referenz.
- Keine vollständige UE5.8-Native-E2E-Abnahme dieses neuen Pfads.
- Das MAC-Face-Energieintegral ist ein numerischer Proxy und **kein**
  vollständiges physikalisches Gesamtenergiekonto.
- Das separate Experiment ist auf maximal 128 Zellen pro Achse und
  2 Millionen Zellen begrenzt; keine GPU/LOD-Skalierung.

Der nächste physikalische Schritt ist die **zeitlich bewegte Körpermaske**
mit swept-volume-konservativer Fluidverdrängung, Druckkraft auf das
bewegte Objekt und nachweislich geschlossenem Impuls-/Energiekonto.
Erst danach kann die echte bidirektionale FSI-Abnahme beginnen.
