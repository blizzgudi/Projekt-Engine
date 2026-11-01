# P2: native PCG-Druckprojektion gegen unabhängige MARIN/SPHERIC-Messreihen

Stand: 01.11.2026. **Numerischer Teilfortschritt – keine P2-Vollabnahme.**

## Mathematische und physikalische Implementierung

Der bisherige 3D-MAC-Drucklöser mit 175/350 Gauss-Seidel-Sweeps pro
Zeitstufe hat bei feineren Netzen nicht zuverlässig auskonvergiert.
Der neue **optionale** Jacobi-vorkonditionierte Konjugierte-Gradienten-Löser
in `include/upc/mac3d.hpp` löst **dieselbe diskrete Poisson-Gleichung**
mit Wasser/Luft-Geisterzell-Faktor 2, unveränderten Wandnormalgeschwindigkeiten
und Restfehlerkontrolle. Für geschlossene Neumann-Komponenten verwendet er
einen mittleren Null-Druckbezug; nicht lösbare Quellen werden zurückgewiesen.

`CoupledMacWater3D` bindet den Löser opt-in per
`set_pcg_pressure_projection(true)` ein.
Der bisherige Gauss-Seidel-Standard bleibt unverändert.
Verifizierte Schritte prüfen die wirkliche nasse Divergenz; misslingt
die Projektion, wird kein partieller Wasser-/Druckzustand veröffentlicht.
Die native Testsuite vergleicht 3D-Hydrostatik bei 8³/12³/16³,
gemischte 3D-Wand- und Luftbedingungen, den langfristig konvergierten
alten Löser und reale gekoppelte MAC/VOF-Updates.
Dieser Poisson-Löser allein erzeugt **keine** vollständigen
Grenzflächenspannungen oder Fluid-Festkörper-Kräfte.

## Unabhängiger Messvergleich: 200 Schritte, zwei 3D-Gitter

Offizielle Quelle: SPHERIC Test 02, MARIN-Dammbruchversuch,
Original-XLS mit 7.395 Zeitpunkten, H1–H4 und P1–P8.
Originale ZIP- und XLS-SHA-256 werden vor jeder Auswertung geprüft.
Der identische native 3D-Experimentfall wurde **neu gerechnet**:
32 × 10 × 10 (dx 0,10 m), 64 × 20 × 20 (dx 0,05 m), je 200 × 0,01 s.
Wasserfüllung, Hindernis, Zeitschritt und Auswertezellen bleiben
unverändert; nur der Drucklöser unterscheidet sich.
Beide Lösungswege verwenden dieselbe wandtangentiale Interpolation.

| Drucksensor | alter GS RMSE, grob (Pa) | neuer PCG, grob (Pa) | alter GS, fein (Pa) | neuer PCG, fein (Pa) |
|---|---:|---:|---:|---:|
| P1 | 5.642 | 4.550 | 18.166 | 3.460 |
| P2 | 5.953 | 4.874 | 18.523 | 3.572 |
| P3 | 5.553 | 4.442 | 19.045 | 3.232 |
| P4 | 5.381 | 4.273 | 20.882 | 3.062 |
| P5 | 5.271 | 4.242 | 16.203 | 3.688 |
| P6 | 5.316 | 4.242 | 16.062 | 3.554 |
| P7 | 5.299 | 4.150 | 16.120 | 3.552 |
| P8 | 5.481 | 4.250 | 16.312 | 3.567 |

Damit sinkt der Druck-RMSE gegenüber dem bisherigen Verfahren
auf **beiden Gittern bei allen 8/8 Sensoren**. Außerdem verringert
räumliche Verfeinerung jetzt die PCG-Druckfehler an **8/8 Sensoren**,
statt sie wie zuvor um ein Mehrfaches zu erhöhen.

Die maximale unmittelbar nach der Druckprojektion protokollierte
nasse Divergenz liegt bei grob 5,72 × 10⁻⁹ s⁻¹ und fein
1,41 × 10⁻⁸ s⁻¹. Die kumulative Wasser-Volumenabweichung liegt
in beiden Native-Läufen unter 1,0 × 10⁻¹³ m³.

**Das ist noch keine experimentelle Vollvalidierung:** Auch der neue
feine Druck-RMSE beträgt zwischen 3.062 und 3.688 Pa (gemeint:
**3.062 bis 3.688 Pascal in deutscher Tausender-Schreibweise,
also rund 3–3,7 kPa**). Die vier feinen Höhenfehler liegen bei
0,1039 / 0,0767 / 0,1400 / 0,1591 m und überschreiten alle
das vorab gesetzte 0,05-m-Ziel.

## Test- und Quellnachweise

- `tests/p2_mac_pcg_pressure_tests.cpp` und
  `tests/p2_live_pcg_coupled_tests.cpp` prüfen die Poisson-
  und echte Live-Solver-Verdrahtung.
- `scripts/p2_marin_pcg_compare.py --fine` prüft vollständige
  jeweils 201 Zustände umfassende native Zeitreihen,
  echte Labormessungen und den historischen GS-Vergleich.
- `tests/p2_marin_pcg_experimental_regression.py` prüft
  neun positive und absichtlich korrumpierte negative Testfälle.
- `scripts/p2_marin_pcg_measured_partial_acceptance.py` prüft
  die evidenztragenden Quell-/Binär- und Log-SHA256.
- Beweise unter `ENGINE TEST STUBE/runs/`:
  `p2_marin_pcg_coarse200_dense.txt`,
  `p2_marin_pcg_fine200_dense.txt`,
  `p2_marin_pcg_experimental_partial.json`,
  `p2_marin_pcg_measured_partial_acceptance.json`.

## Bestehende Sperren einer P2-Vollabnahme

Verifizierte Poisson-Restfehler sind **nicht** verifizierte
Fluid-Grenzspannungen. Bis zu einer echten Vollabnahme fehlen:
spannungsrichtige freie Wasser/Luft-Grenzflächen, quantitativ
korrekte experimentelle Druck- und Wasserstandskurven,
unabhängige konvergierende 3D-Gitterstudie über die geforderten
6 s, geometrisch unabhängig aufgelöste Transducer,
dynamische bidirektionale Impulsübertragung an deformierbare und
starre Körper sowie globale physikalische Energie-/Impulsschlüsse.
**P2 bleibt INCOMPLETE und ALL_COMPLETE=False.**
