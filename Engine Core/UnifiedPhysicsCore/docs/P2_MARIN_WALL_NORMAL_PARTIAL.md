# P2: MARIN P1–P8 Wandnormal-Druckrekonstruktion (Teilnachweis)

Stand: 01.11.2026. **P2 OFFEN / experimenteller Druckabgleich FAIL.**
Der neue Modus ist **opt-in**, verändert weder den 3D-MAC/VOF-Solver noch
die PCG-Druckprojektion und ist ausdrücklich **keine** validierte
Wandtraktion oder vollständige CFD-/FSI-Abnahme.

## Motivation und unabhängige Quelle

Die ursprüngliche MARIN-Auslesung nimmt die MAC-Zelldruckwerte an einer
ersten flüssigkeitsseitigen Zellmitte und interpoliert bilinear entlang
der Wand. Die Normaldistanz zur tatsächlichen Wand bleibt dabei
ungekorrigiert. Die Originalgeometrie, acht Drucksonden und die
6-Sekunden-Anforderung stammen aus dem lokalen Originaldokument
`references/SPHERIC_Test02/test_case_2_v1p1.pdf`, insbesondere S. 2,
5 und 8 (SHA256:
`36b56e3ad3348efcf845852393f5d5cb0edbe5e46a002346693aa40f29d9e01a`).
Quelle: https://www.spheric-sph.org/tests/test-02 .
Das originale ZIP bleibt unverändert (SHA256
`a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1`).

## Tatsächliche Implementierung

- `include/upc/p2_marin_wall_normal_pressure3d.hpp`: auf beiden
  flüssigkeitsseitigen Normalebenen separat bilineare Tangentialauslesung
  mit expliziter Festkörper-/Trockenmaskierung und VOF-Gewichtung.
  Anschließend lineare Rückextrapolation zum geometrischen Wandort:
  `p_wall = p_first + (wall - centre_first)/dx * (p_second - p_first)`.
- Bei trockener oder fester zweiter Ebene wird **explizit** auf den
  ersten Druckwert zurückgefallen; keine erfundenen Flüssigkeitsdrücke.
  Nichtfinite Drücke/VOF-Werte, ungültige Abstände und Indizes werden
  zurückgewiesen. Pro Zeitschritt sind nasse, extrapolierte und
  zurückgefallene Sonden zählbar.
- `benchmarks/p2_marin_dambreak_heights.cpp`: optionaler
  `normal_extrap`-Modus mit eigenen Header-/CSV-Feldern; der
  bisherige PCG-/VOF-Transport bleibt unverändert.
- `tests/p2_marin_wall_normal_pressure3d_tests.cpp`: affine
  hergestellte 3D-Druckfelder auf 32×10×10 und 64×20×20,
  quadratische Normalfehler-Abnahme, Trocken-/Fallback-/
  Nichtfinite-Negativfälle sowie echte 3D-MAC/PCG-Samples.

## Reale native Ergebnisse (kein 6-s-Full-Gate)

MSVC Release / Ninja: Build erfolgreich; **109 native Assertions PASS**.
Die vollständige Suite: **119/119 CTest PASS** (26,29 s).
Ein neuer nativer 200-Schritt-Lauf auf 32×10×10, dx=0,1 m, dt=0,01 s,
bis **2,00 s** wurde ausgeführt. Das 3D-MAC/PCG-Fluid und alle H1–H4-
Höhen, die Wassermasse, Divergenz, Zeiten und die Anzahl nasser
Drucksonden stimmen **numerisch identisch** mit der bestehenden
PCG-Kontrolltrajektorie überein (maximale Differenz 0).
Der neue Druckmodus ändert nur die Druckauslesung. Zwischen 0 und 8
Drucksonden werden pro Zustand normal extrapoliert; bis zu 2 nasse
Sonden fallen auf den ersten Druckwert zurück.
Sechs absichtlich korrumpierte Traces wurden vom unabhängigen
Python-Audit korrekt zurückgewiesen.

**Negative experimentelle Evidenz:** Gegen die unveränderten
MARIN-Originalmesswerte ist der RMSE in **allen acht** Kanälen
schlechter geworden. Keine Nachkalibrierung, keine Ergebnisfilterung.

| Kanal | Vorher, tangential (Pa) | Neuer Normal-Proxy (Pa) | Bewertung |
|---|---:|---:|---|
| P1 | 4550,48 | 4617,76 | schlechter, FAIL |
| P2 | 4873,84 | 4939,79 | schlechter, FAIL |
| P3 | 4441,69 | 4461,92 | schlechter, FAIL |
| P4 | 4272,58 | 4295,14 | schlechter, FAIL |
| P5 | 4242,11 | 4996,82 | schlechter, FAIL |
| P6 | 4241,68 | 4932,49 | schlechter, FAIL |
| P7 | 4150,43 | 4829,09 | schlechter, FAIL |
| P8 | 4250,25 | 4951,95 | schlechter, FAIL |

Der neue Auslesemodus ist deshalb **nicht** als physikalisch
verbesserte MARIN-Druckvorhersage freigegeben. Die starken Fehler
zeigen, dass Druckrandbedingung, freie Oberfläche, Hindernis-
Voxelisierung, fehlende Luftphysik und Messflächenmodell weiterhin
unabhängig untersucht werden müssen.

## Hashes / Reproduktion

| Artefakt unter `D:\\Projekt Engine` | SHA256 |
|---|---|
| `ENGINE TEST STUBE/runs/p2_marin_wall_normal_coarse200.txt` | `faa2122318a5e8051a4f3eb300095a16a9a9ff62b96d3135912ddaf86fbe40c5` |
| `ENGINE TEST STUBE/runs/p2_marin_wall_normal_partial.json` | `c608d9f8b459494e194ef671438c9eff452e2e8b27064950ae2bdbe7ea64d44d` |
| `ENGINE TEST STUBE/runs/p2_marin_wall_normal_native.txt` | `480d52bec069ebe19e87e50bee3a548ab7f7b2c2942151244441b94585ab335d` |
| `ENGINE TEST STUBE/runs/p2_marin_wall_normal_ctest.txt` | `7314074450da6277bedb7235ddf58b4aa98d5fd3606dea9070c99d61a6b5214c` |
| `ENGINE TEST STUBE/runs/p2_marin_wall_normal_phase_gate.txt` | `ab300bb727908749ec3a803f1e6b7f90f660ad67714cbabc643011315b881408` |

Der JSON-Teilbericht enthält zusätzlich Source-, Binary-, Log- und
Messdaten-SHA256, alle acht RMSE, Negativfälle, Sensor-Fallback-
Statistiken und die unberührten Solver-Signale. Der Reproduktionsaufruf
`scripts\\p2_marin_wall_normal_build.cmd --run` baut und testet
nativ und berechnet einen neuen 2-s-Trace. Ohne `--run` wird nur
die vorhandene Evidenz erneut geprüft. Kein kostspieliger 6-s-Lauf
wurde für diesen isolierten Ausleseversuch wiederholt.

## Verbindlicher Status

`scripts/phase_gate.py` meldet:
`GATES_PASS=P0`, `GATES_INCOMPLETE=P1,P2,P3,P4,P5,P6,P7,P8`,
`ALL_COMPLETE=False`. P1 erscheint wegen der alten
CMakeLists-Quellhashbindung des historischen P1-Reports als
PARTIAL, nicht wegen eines neuen physikalischen P1-Testfehlers.

**P2 bleibt PARTIAL.** Offen: echte stresskonsistente Wasser-Luft-
Grenze, messkonvergente P1–P8/H1–H4 über >=6 s und mindestens drei
unabhängige Gitter, dynamische bidirektionale FSI, globale
physikalische Massen-/Impuls-/Energiebilanzen und unabhängige
Native-/Mess-E2E-Vollabnahme.
