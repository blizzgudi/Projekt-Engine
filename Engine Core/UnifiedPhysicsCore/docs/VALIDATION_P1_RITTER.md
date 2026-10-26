# P1 – analytische Referenzprüfung des 2D-Flachwassersolvers

Stand: 2026-10-26. Test: tests/ritter_reference_tests.cpp, CTest-Name
physics_ritter_analytical_reference.

## Unabhängiges mathematisches Referenzproblem

Ritter-Lösung (1892), idealer Dammbruch in ein trockenes horizontales Bett:
eindimensionale Flachwassergleichungen, ohne Reibung, ohne Anfangsgeschwindigkeit.
Links ist h_L = 1 m, rechts h_R = 0 m, die Trennstelle liegt bei x_0 = 6 m.
Schwerebeschleunigung g = 9.81 m/s²; Prüfzeit t = 0.25 s; Wände bei
x = 0 und x = 12 m, daher bis t = 0.25 s außerhalb des Ausbreitungsbereichs.

Mit c = sqrt(g h_L) und ξ = (x-x_0)/t gilt die exakte Tiefe:

- ξ <= -c: h = h_L und u = 0.
- -c < ξ < 2c: h = (2c-ξ)^2 / (9g) und u = 2(c+ξ)/3.
- ξ >= 2c: h = 0 und hu = 0.

Die Implementierung der Referenzformel ist von der numerischen Rusanov-Flussroutine
getrennt. Verglichen werden Tiefen- und Impuls-L1-Mittelwerte, Wassererhaltung,
Positivität, Wellenfrontposition und Konvergenz unter Gitterverfeinerung.

## Gemessene MSVC-Release-Ergebnisse

| dx (m) | mittlerer Tiefenfehler (m) | mittlerer Impulsfehler (m²/s) | Volumenabweichung (m³) | Fehler der numerischen Front (m) |
|---:|---:|---:|---:|---:|
| 0.100 | 0.00871395 | 0.01964483 | 7.77e-14 | 0.3160 |
| 0.050 | 0.00647581 | 0.01441295 | 5.64e-14 | 0.3910 |
| 0.025 | 0.00444597 | 0.01029003 | 3.68e-14 | 0.4035 |

Rohdaten: ENGINE TEST STUBE/runs/ritter_analytical_reference_20261026.csv.
Sechs CTest-Suites bestanden nach MSVC-CMake-Release-Build.

**Wichtige Einschränkung:** Die Frontposition wird anhand einer
Tiefenschwelle h > 0.002 m gemessen und mit der mathematisch *trockenen*
Front verglichen. Dadurch ist ein Teil des Fehlers definitionsbedingt.
Der Frontfehler sinkt im untersuchten Gitterbereich nicht monoton und
liegt auf dem feinsten Gitter bei rund 0.40 m. Der grobe 0.50-m-Testguard
dient nur zur Erkennung katastrophaler Regressionen, nicht zur
Abnahme der Frontgenauigkeit. Die Depth-/Momentum-Fehler sinken dagegen
bei Verfeinerung. Die P1-Phase bleibt PARTIAL, bis ein sinnvoll
schwellenkorrigierter Frontvergleich, eine reproduzierbare Genauigkeitsgrenze
und weitere Referenzfälle geprüft wurden.

Die hydrostatische Druckformel p = ρgh, Stillwasser und vollständig
blockierende Hindernisse sind in tests/core_tests.cpp geprüft.
Das ist kein 3D-Wasser- oder CFD-Abnahmenachweis.

## Nächste P1-Schritte

Schwellenabhängigen Frontort gegen eine ebenfalls schwellenabhängige
analytische Position auswerten. Zusätzliche Tests mit glattem Anfangszustand
und geschlossenen Wänden; Druck- und Impulsbilanz prüfen. Bei Bedarf
numerischen Wet-Dry-Riemann-Flux verbessern, statt die Toleranz
willkürlich zu erhöhen.
