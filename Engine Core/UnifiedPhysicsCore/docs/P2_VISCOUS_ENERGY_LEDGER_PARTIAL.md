# P2 – diskrete 3D-MAC-Viskositäts-Energiebilanz (Teilabnahme)

Stand: 26.10.2026. **P2 = PARTIAL**; keine Vollabnahme von P2–P8.

## Implementierter Umfang

In `include/upc/viscous_mac3d.hpp` erhält der bestehende
`MaskedMacMomentumLedger3D` sieben zusätzliche **rein diagnostische**
Komponentenvektoren für die explizite maskierte Viskositätsstufe:
Änderung der aktiven Face-Quadratsumme, interner Gradientverlust,
Festkörper-Gradientverlust, Außenwand-Gradientverlust,
vorgegebene Wandarbeit, explizite Euler-Korrektur und Residuum.
Ohne übergebenen Ledger wird keine Energiebuchhaltung durchgeführt.
Die numerische Geschwindigkeitsaktualisierung bleibt unverändert.

Für jede Komponente gilt die exakt diskrete Identität (in m²/s²):

```text
delta(0.5 * sum_active_faces(v^2))
 = - alpha * sum_active_active_edges((vi-vj)^2)
   - alpha * sum_active_solid_edges(vi^2)
   - alpha * sum_active_exterior_edges(vi^2)
   + alpha * sum_active_solid_edges(vi*vwall_tangent)
   + 0.5 * sum_active_faces((v_after-v_before)^2)
```

`alpha=nu*dt/(dx²*viscosity_substeps)`. Aktive interne Nachbarn
werden paarweise nur einmal gezählt. Air-only-Faces verwenden weiterhin
den Neumann-Nullgradient-Proxy. Die letzte positive Größe ist die
**exakte explizite Euler-Korrektur**, keine physikalische Wärmequelle.
Die algebraische Identität gilt für die konkrete Face-Stencil-Methode,
nicht für eine stresskonsistente Navier-Stokes-/FSI-Energieerhaltung.
Insbesondere sind Face-Dualvolumina, VOF, Druck, Advektion und
Schwerkraft hier nicht als physikalische Gesamtenergie enthalten.

## Unabhängige Referenz

Ein 10³-Raster, dx=0,1 m, nu=0,012 m²/s, dt=0,03 s, Festkörper x<4,
Wasser x>=4, tangentiale y-Wandgeschwindigkeit 0,4 m/s und ein
vorbelegtes y-Face (4,5,5)=0,25 m/s ergeben:

- `alpha=0,036`, genau 90 wandbenachbarte y-Faces.
- Center: `0,25 + 0,036*(0,4-6*0,25) = 0,2104 m/s`.
- Fünf benachbarte aktive Faces erhalten den ursprünglichen Impuls;
  vier davon liegen ebenfalls an der Wand und erhalten zusätzlich
  `0,036*0,4`; der fünfte liegt im Inneren.
- Interner Gradientverlust `5*0,036*0,25² = 0,01125`.
- Solid-Gradientverlust `0,036*0,25² = 0,00225`.
- Wandarbeitsproxy `0,036*0,25*0,4 = 0,0036`.
- Explizite Euler-Korrektur mit 85 weiteren Wand-Faces:
  `0,5 * ((0,036*(0,4-6*0,25))²
  + 4*(0,036*(0,4+0,25))²
  + 85*(0,036*0,4)² + (0,036*0,25)²) = 0,0107325`.
- Erwartete Änderung: `+0,0008325` in der diskreten
  `0,5*sum(v²)`-Diagnose. Die positive Änderung ist **kein**
  physikalischer FSI-Energiebeweis.

Der neue native Test kontrolliert zusätzlich stationäre Wand mit
Dissipation, 8 adaptive Viskositäts-Substeps, vorzeichenabhängige
Wandarbeit, freie Oberflächen, drei Achsen, exakte Nullviskosität und
ungültige Eingaben ohne Ledger-Mutation.

## Reproduktion und Grenzen

Im vorhandenen MSVC Developer Environment:

```powershell
cmake --build build --config Release --parallel 5
ctest --test-dir build --output-on-failure --parallel 5
C:\Python314\python.exe scripts\p2_viscous_energy_acceptance.py
C:\Python314\python.exe scripts\phase_gate.py
```

Der maschinenlesbare Teilnachweis liegt unter
`D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_viscous_energy_partial.json`
mit nativer Binär-SHA256, Quell-SHA256, echten CTest-Ergebnissen und
`phase_complete=false`. Der Test ist **nicht** mit einem
unabhängigen gemessenen 3D-Experiment zu verwechseln.

**Weiter offen:** stresskonsistente freie Wasseroberfläche,
bewegte Festkörper mit echter viskoser Impuls-/Energierückgabe,
unabhängige gemessene 3D-CFD-Referenzen und Konvergenz,
globale gekoppelte Energie-/Impulsbilanz sowie UE5.8-Native-E2E
für diese Änderung. P2–P8 bleiben PARTIAL.
