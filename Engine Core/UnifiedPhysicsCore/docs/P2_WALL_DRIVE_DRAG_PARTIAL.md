# P2 – Statische Wand: numerische Drive-/Drag-Zerlegung (Teilabnahme)

Stand: 26.10.2026. **P2 = PARTIAL**, P3–P8 weiterhin offen.

## Motivation und Implementierung

Der maskierte 3D-MAC-Viskositätsoperator lieferte bisher nur die
kombinierte numerische Festkörper-Grenzquelle `alpha * (v_ghost - v_face)`.
Jetzt werden beide Beiträge **getrennt** geführt:

- `solid_wall_drive_sum_m_s += alpha * v_ghost`
- `solid_wall_drag_sum_m_s -= alpha * v_face`
- `solid_boundary_source_sum_m_s = drive + drag`

Das gilt pro aktiver tangentialer MAC-Face-Nachbarschaft zu einem
statischen Festkörper. Die vorhandene numerische Grenzquellen- und
Face-Summenbilanz bleibt unverändert. Im gekoppelten `CoupledMacWater3D`
werden beide Beiträge über die Substeps akkumuliert und mit dem bisherigen
**Proxy-Faktor** `rho * dx^3` bereitgestellt. Der inviskose Pfad setzt
die neuen Diagnosen wie die bisherigen explizit auf null.

**Keine physikalische Wandkraft:** Die Größe beschreibt lediglich den
diskreten Beitrag zur Summe staggered MAC-Face-Geschwindigkeiten.
Weder reale viskose Spannung an einer Grenzfläche noch Impulsübertragung
auf einen bewegten Körper, Druck, Advektion oder Schwerkraft sind darin
physikalisch vollständig bilanziert.

## Unabhängiger Stencil-Test

Raster: 10×10×10, dx = 0,1 m, nu = 0,012 m²/s, dt = 0,03 s.
Festkörper bei x<4, Wasser bei x>=4, vorgeschriebene tangentiale
Wandgeschwindigkeit in y = 0,4 m/s. Daraus folgt:

- `alpha = nu*dt/dx² = 0,036`.
- 90 tangentiale y-Faces an der x-Grenze.
- Unabhängig berechneter Wand-Drive: `90 * 0,036 * 0,4 = 1,296 m/s`.
- Wird genau ein benachbartes y-Face mit `v_face=0,25 m/s` vorbelegt,
  beträgt der unabhängige Wand-Drag `-0,036 * 0,25 = -0,009 m/s`.
- Unabhängiger 6-Nachbar-Einzelschritt an diesem Face:
  `0,25 + 0,036 * (0,4 - 6*0,25) = 0,2104 m/s`.

Die native Testdatei überprüft darüber hinaus alle drei Achsen,
Drive+Drag=Festkörperquelle, aktives Face-Summen-Closure, Umkehr der
Wandgeschwindigkeit, adaptive Substeps, VOF-Massenkonstanz im laufenden
8×8×6-Solver und `nu=0`-Identität.

## Reproduktion und Evidenz

Aus `D:\Projekt Engine\Engine Core\UnifiedPhysicsCore` mit MSVC-Developer-Umgebung:

```powershell
cmake --build build --config Release -j 5
ctest --test-dir build --output-on-failure
C:\Python314\python.exe scripts\p2_wall_drive_drag_acceptance.py
```

Das Teil-Gate ruft die vorhandene Momentum-Ledger-Abnahme erneut auf,
prüft den nativen Test und alle CTest-Suites, die konservative
`phase_gate.py`-Aussage sowie SHA256 der relevanten Quellen.
Maschinenlesbarer Nachweis:
`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_wall_drive_drag_partial.json`.
`phase_complete=false` bleibt zwingend.

## Unverändert offene P2-Vollkriterien

Stresskonsistente dynamische Wasser-Luft-Grenze, tatsächlich bewegte
Festkörper mit physikalischer Wandimpuls-/Energierückgabe,
unabhängige gemessene 3D-Referenzfälle, Konvergenz und
UE5.8-Native-E2E für diese Erweiterung. Die Teilabnahme behauptet
keinen vollständigen CFD-/FSI-/AAA-Produktstand.
