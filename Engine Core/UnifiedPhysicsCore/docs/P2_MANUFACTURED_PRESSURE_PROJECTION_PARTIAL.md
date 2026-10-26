# P2 – 3D-MAC-Druckprojektion: Manufactured-Pressure-Teilnachweis

Stand: 27.10.2026. **Nur P2-Teilabnahme, kein vollständiges CFD-/FSI-Gate.**

## Prüfumfang

Der vorhandene `MacProjection3D::project()` wird ohne Änderung seiner
numerischen Implementierung auf drei echten 3D-Rastern `8×7×6`,
`12×11×10` und `16×15×14` gegen ein unabhängiges analytisch
vorgegebenes Druckfeld getestet. Je Raster gibt es zwei unterschiedliche
Topologien: (1) eine Flüssigkeits-/Luft-Grenze mit halbzelligem
Dirichlet-Drucksprung und (2) einen vollständig geschlossenen
Festkörperrand mit reiner Neumann-Bedingung. In beiden Fällen enthält
das Wasser einen festen internen 1-Voxel-Hinderniskörper.

Das vorgegebene Druckfeld in Pascal ist

`p(x,y,z) = 250 + 17 sin(0.27x) + 13 cos(0.41y) + 11 sin(0.23z) + 0.75xy/nx`

auf Flüssigkeitszellen; Luftdruck ist null. Für jede MAC-Face wird
unabhängig die Anfangsgeschwindigkeit
`u* = dt/(rho dx) * gamma * (p_B-p_A)` erzeugt, mit `gamma=1` bei
Wasser/Wasser, `gamma=2` bei Wasser/Luft und `u*=0` an festen Wänden.
Bei `dt=0.02 s`, `rho=1000 kg/m³`, `dx=0.08 m` muss die Projektion
die erzeugte Druckgradienten-Geschwindigkeit wieder entfernen. Die
Python-Abnahme berechnet die anfängliche 3D-Divergenz unabhängig aus
dem analytischen Feld, statt nur die C++-Ausgabe zu übernehmen.

Für geschlossene, reine Neumann-Domänen wird der Druck ausschließlich
**bis auf eine additive Konstante** verglichen. Die Projektionsroutine
muss keine künstliche absolute Druck-Referenz erfinden. Zusätzlich
werden der Divergenzrest, alle MAC-Faces, der Nullfluss an Festkörpern
und eine zweite Projektion kontrolliert.

## Gemessene MSVC-Release-Ergebnisse

| Randfall | Raster | Nasszellen | Anfangsdivergenz (1/s) | Restdivergenz (1/s) | Max. Druckfehler (Pa) |
|---|---|---:|---:|---:|---:|
| Wasser/Luft | 8×7×6 | 99 | 1.8171340293 | 8.72e-11 | 2.34e-7 |
| Wasser/Luft | 12×11×10 | 647 | 1.7789293313 | 9.22e-11 | 8.65e-7 |
| Wasser/Luft | 16×15×14 | 2027 | 1.6963530063 | 9.29e-11 | 1.88e-6 |
| geschlossen | 8×7×6 | 119 | 0.0376611531 | 8.30e-11 | 1.41e-7 |
| geschlossen | 12×11×10 | 719 | 0.0287993922 | 8.73e-11 | 4.38e-7 |
| geschlossen | 16×15×14 | 2183 | 0.0284087672 | 9.06e-11 | 9.87e-7 |

Alle festen Rand-Faces hatten `0 m/s` Fluss. Maximaler Restwert
einer projizierten MAC-Face lag unter `6e-11 m/s`.

- Native Quelle: `tests/p2_pressure_projection_manufactured3d_tests.cpp`
- Native Binary: `build/upc_p2_manufactured_projection_tests.exe`
- Native Lauf: **38.296 Assertions PASS**
- Vollständige MSVC-Release-CTest-Suite: **40/40 PASS**
- Unabhängiges Python-Gate:
  `scripts/p2_pressure_projection_acceptance.py`
- Maschinenlesbarer Report mit exakten Source-, Binary- und Log-SHA256:
  `../../ENGINE TEST STUBE/runs/p2_manufactured_projection_partial.json`
- Vollständiger CTest-Log:
  `../../ENGINE TEST STUBE/runs/p2_pressure_projection_ctest_20261027.txt`

## Grenzen und nächste Schritte

Das ist ein starker **diskreter manufactured-solution-Identitätstest** des
vorhandenen 3D-Poisson-Projektors mit internen Hindernissen und gemischten
Randbedingungen. Es ist weder ein experimenteller 3D-Strömungsvergleich
noch eine Validierung von Impulsrückwirkung, turbulenter
Mehrphasenströmung, beweglichen Festkörpern, globaler mechanischer
Energieerhaltung oder stresskonsistenter Wasser-Luft-Viskosität.
Auch Unreal-5.8-Native-E2E wurde hier nicht erneut ausgeführt.

Die bekannten P2-Vollkriterien (bewegliche FSI, echte 3D-Messreferenzen,
freie Oberflächenspannungsrandbedingungen, vollständige Impuls-/Energie-
Kopplung) bleiben offen. **`phase_complete=false`; P2–P8 bleiben
PARTIAL.** Ein Prototyp-/Unit-PASS ersetzt keinen vollständigen
Phasenabschluss.
