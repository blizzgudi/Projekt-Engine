# P2 Teilnachweis: dynamische lokale PLIC-Normalen im 3D-MAC-PCG-Livepfad

Stand 2026-11-01. **P2 PARTIAL, keine Vollabnahme.**

## Implementierung

`CoupledMacWater3D` kann jetzt optional die vorhandene streng
volumenkonsistente lokale PLIC-Normalenrekonstruktion **vor jeder echten
PCG-Druckprojektion** aus dem aktuellen 3D-VOF-Feld berechnen. Einschliesslich
einer zweiten Projektion nach VOF-Transport, falls der Aufrufer
`step_verified_reprojected` verwendet. Der bisherige externe Normalenmodus
und der Legacy-/Achsmodus bleiben standardmaessig unveraendert.

Reihenfolge: `set_pcg_pressure_projection(true)`,
`set_auto_planar_vof_pcg_normals(true, 1e-7)`,
`set_planar_vof_pcg_geometry(true)`, danach `step_verified(...)`.
Der Fit erfordert eine lokale 3x3x3-Nachbarschaft, die zu **einer**
volumengleichen Ebene passt. Bei gekruemmten, gestörten oder
topologisch veraenderten VOF-Feldern erfolgt eine explizite Ausnahme
und der transaktionale Step rollt zurueck. Die Geometrie kann
nur im verifizierten Live-Pfad verwendet werden. Diagnostik:
`last_auto_planar_vof_fitted_cells()` und
`last_auto_planar_vof_patch_residual()`.

## Native und unabhaengige Tests

Reproduktion unter Windows im einzigen Projekt `D:\Projekt Engine`:
`scripts/p2_live_auto_planar_build.cmd` baut mit MSVC Release, startet
das neue native Testbinary und fuehrt anschliessend die **gesamte**
CTest-Suite aus. `scripts/p2_live_auto_planar_acceptance.py` validiert
zwei unabhaengige analytische, exakt per Cube-Clip erzeugte oblique
3D-VOF-Ebenen und reproduziert das Testbinary, verwirft manipulierte
Testprotokolle und archiviert SHA256 aller beteiligten Quellen,
Binaerdateien und Testlogs.

Die Tests pruefen fuer 8^3 und 12^3: echte lokale Normalenfits,
PCG-Druckprojektion, Wasser-Masse, Rekonstruktion nach geaenderter
VOF-Ebene, separate echte Post-VOF-Zweitprojektion mit erneutem Fit,
zwei-partielle Schnittfaces und transaktionale
Ablehnung einer absichtlich nichtplanaren Zelle. Hinzu kommen
Konfigurations-Negativfaelle (ungueltige Toleranzen, unverified
step, widerspruechliche Normalquellen). Diese Tests beweisen
**keine** allgemeine dynamische Grenzflaechenstabilitaet.

Erste native Messwerte:
- 8^3: 104 gefittete Teilzellen, 96 Druckgrenzfaces,
  Projektion 2.654347146810801e-16 s^-1, zweiter Frame
  6.7918489842638818e-16 s^-1.
- 12^3: 166 gefittete Teilzellen, 165 Druckgrenzfaces,
  Projektion 8.5305852788283062e-16 s^-1, zweiter Frame
  7.1687468820565235e-16 s^-1.
- 60/60 native Assertions, 126/126 CTest-Suites PASS
  (MSVC-Release-Build; letzte CTest-Ausfuehrung 7.14 s,
  erster Vollrebuild 42.57 s).

Artefakte:
`ENGINE TEST STUBE/runs/p2_live_auto_planar_build.txt`,
`p2_live_auto_planar_native.txt`, `p2_live_auto_planar_ctest.txt`
und `p2_live_auto_planar_partial.json`.

## Negativbefunde / offene Vollkriterien

Die erste native Testfassung scheiterte an einer zu engen
Testdaten-Auswahl (>0.2 und <0.8), weil eine synthetische Ebene
nur Teilzellen ausserhalb dieses Intervalls lieferte. Der Test
wurde auf jede echt partielle Zelle korrigiert und danach
erneut nativ gebaut und bestanden. Das ist kein physikalischer
Solverfehler und wird nicht als solcher ausgegeben.

Der lokale Fit ist bei generischen advektierten, nichtplanaren
Grenzen absichtlich **nicht** universell. Keine Oberflaechenspannung,
keine viskose Traktionskontinuitaet von Wasser/Luft, keine
gekoppelte dynamische Fluid-Festkoerper-Impulsrueckwirkung.
Die vorhandene echte SPHERIC/MARIN-6-s-Vergleichsmessung bleibt
negativ: H1-H4-RMSE 0.136525/0.091500/0.148245/0.224399 m,
jeweils ueber 0.05 m. Keine neuen 6-s-/Dreigitter-Läufe,
keine experimentelle P1-P8-Druckkalibrierung und kein
Unreal-5.8-Native-E2E dieses Pfades. Globaler Energie- und
Impulsabschluss fehlt.

`phase_complete=false`; `phase_gate.py` muss P2 weiter
als **PARTIAL** melden. Die 126 CTests sind Regressionstests,
keine P2-Vollabnahme.
