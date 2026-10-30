# P2 Teilabnahme: dreidimensionale fraktionale Druckarbeit (30.10.2026)

## Abgrenzung

Ein vorhandener, isolierter CPU-Operator `P2MovingBoxPressureTraction3D::audit`
wird erstmals fuer simultane Translation eines **fraktionalen** 3D-Quaders
in allen drei Achsen mit einem separablen, dreidimensionalen
Zellmittel-Druckfeld quantitativ gegen eine unabhaengige analytische
Finite-Volumen-Referenz getestet.

Die Ausgangsbox reicht in Zelleinheiten von (3.2,3.1,3.3) bis
(5.7,5.6,5.8). Ihre Kantenlaengen sind je 2.5 Zellen.
Die signierte Translation in Zelleinheiten ist
(+0.11,-0.08,+0.09) oder deren Gegenteil. Alle sechs Grenzflaechen
bleiben dabei innerhalb ihrer Ausgangszellen; dies ist eine
bewusst begrenzte Subzellen-Validierung ohne Zellgrenzuebertritt.

Druck: `p(i,j,k) = 100 + 12(i+0.5) - 7(j+0.5) + 5(k+0.5) Pa`.
Fuer jede Achse ergibt die Differenz der Druckzellmittel an den
beiden gegenueberliegenden Flaechen genau zwei Zellgradienten.
Mit Flaeche `A = (2.5 dx)^2` gilt analytisch:

- `F = (-150, +87.5, -62.5) * dx^2 N`.
- `W = F dot Delta_x = -29.125 * dx^3 J` fuer die positive Translation.
- `W_fluid = -W_solid` als geometrische Diagnoserechnung.
- Die gesamte Oberflaeche betraegt `37.5 dx^2 m^2`.

Der native Test kontrolliert zudem drei Einzelachsen-Translationen,
beide Bewegungsrichtungen, unveraenderte Druckeingaben,
die nur **diagnostische** Gegenimpulsbuchung, die Arbeit aus
Mittelpunkt-Traktion, Gauge-Invarianz bei +500 Pa, endliche Drehmomente
und Negativfaelle fuer Druckform, NaN und geometrische CFL.
Die zwei Gitter (8^3 bei dx=0.1 m; 12^3 bei dx=0.125 m) liefern
vier unabhaengige dreiachsige Referenzfaelle.

## Reproduktion und echte Nachweise

- Nativer Test: `tests/p2_fractional_pressure_work3d_tests.cpp`.
- Reproduzierbarer MSVC-Release-Build + vollstaendige CTest-Suite:
  `scripts/p2_fractional_pressure_work_full_build.cmd`.
- Eigenstaendige Python-Abnahme:
  `scripts/p2_fractional_pressure_work_acceptance.py`.
- JSON und vollstaendiges CTest-Protokoll unter
  `ENGINE TEST STUBE/runs/p2_fractional_pressure_work_partial.json` und
  `ENGINE TEST STUBE/runs/p2_fractional_pressure_work_full_ctest_20261030.txt`.
- Die neue Testdatei wird bewusst separat kompiliert, ohne historische
  CMake-Dateien oder die P0/P1-Quellfingerprints zu veraendern.

## Grenzen

Der Test validiert ausschliesslich einen diskreten, zellmittelbasierten
Drucklast- und Druckarbeitsoperator mit **vorgegebener** Subzellenbewegung.
Die analytische Referenz ist kein experimentelles CFD-Messergebnis.
Der Gegenimpuls wird nicht in eine nachprojizierte Fluid-FSI-Bilanz
eingebracht. Keine zeitlich gekoppelte Navier-Stokes-Dynamik,
keine globale physikalische Impuls-/Energieerhaltung,
keine stresskonsistente freie Wasseroberflaeche und keine neue
Unreal-5.8-Native-E2E-Abnahme. P2 bleibt INCOMPLETE, ebenso P3-P8.
