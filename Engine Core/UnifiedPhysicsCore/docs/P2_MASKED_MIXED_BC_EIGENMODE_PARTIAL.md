# P2 Teilnachweis: gemischte Dirichlet-/Neumann-Randbedingungen im 3D-MAC-Viskositätsoperator

Stand: 26.10.2026. **Numerischer Teilnachweis; P2 NICHT abgeschlossen.**

## Umfang

Neuer nativer MSVC-Test `tests/viscous_masked_eigenmode3d_tests.cpp` mit drei unabhängigen 3D-Rastergrößen:
- `(nx,ny,nz)=(8,7,6)`, **3** nasse x-Spalten;
- `(10,9,8)` mit **5** nassen x-Spalten;
- `(12,11,10)` mit **7** nassen x-Spalten.

In jedem Gitter sind `x<2` feste Voxel (stationäre Dirichlet-Ghost-Wand), die folgenden `L` Spalten sind Wasser, rechts davon nur Luft (diskrete Neumann-Nullgradient-Proxy). Die übrigen Außenwände verwenden No-Slip-Dirichlet. Ein ausschließlich in y-Richtung aktives staggered-Face-Feld wird mit einer **analytisch eigenständigen diskreten 3D-Eigenfunktion** initialisiert:

```text
u_y(x,y,z) = sin((x-left+1)*pi/(2*L+1))
           * sin(y*pi/ny) * sin((z+1)*pi/(nz+1))
lambda = 2(cos(pi/(2*L+1))-1)
       + 2(cos(pi/ny)-1)
       + 2(cos(pi/(nz+1))-1)
alpha = nu*dt/(dx^2*N),  N = ceil(nu*dt/(0.15*dx^2))
u_y(N) = (1+alpha*lambda)^N * u_y(0)
E_face(N)/E_face(0) = (1+alpha*lambda)^(2*N)
```

Der gemischte x-Rand hat `u_{-1}=0` am Festkörper und `u_L=u_{L-1}` an Luft. Die native Prüfung kontrolliert alle aktiven und inaktiven y-Faces, Nullgeschwindigkeit der x/z-Faces, die diskrete Face-Energiebilanz sowie einen separat analytisch abgeleiteten **inhomogenen Couette-Einzelschritt**: Nur die erste Wasserschicht erhält den Zusatz `alpha*u_wall`. `nu=0` ist bitgenau ein No-op.

## Reproduktion

Im vorhandenen Visual-Studio-Developer-Environment:

```powershell
cmake --build "Engine Core/UnifiedPhysicsCore/build" --config Release --target upc_p2_masked_eigenmode_tests
& "Engine Core/UnifiedPhysicsCore/build/upc_p2_masked_eigenmode_tests.exe"
& "C:/Python314/python.exe" "Engine Core/UnifiedPhysicsCore/scripts/p2_masked_mixed_bc_acceptance.py"
& "C:/Python314/python.exe" "Engine Core/UnifiedPhysicsCore/scripts/phase_gate.py"
```

Das Python-Gate rekonstruiert `lambda`, `alpha`, den Mehrfachschritt-Faktor und das Energieverhältnis **unabhängig** aus den drei CSV-Zeilen. Es prüft die nativen Assertion-Zahlen, Quell-/Test-/Skript-Hashes und den nativen Executable-SHA256. Nur bei echten grünen Checks gilt `partial_accepted=true`.

Maschinenlesbarer Nachweis: `D:/Projekt Engine/ENGINE TEST STUBE/runs/p2_masked_mixed_bc_partial.json`.

## Grenzen / Abnahme

Dieser Nachweis belegt einen **exakten diskreten Operator** unter gemischten stationären Randbedingungen, **keine** stresskonsistente reale freie Oberfläche, **keine** bewegte Geometrie, **keine** physikalische Fluid-Solid-Reaktionskraft, **keine** Laborreferenz und **keine** globale Fluidenergie. Die bisherige Live-VOF-Kopplung bleibt unverändert. P2 bleibt `PARTIAL`, P3–P8 ebenso. Ein bestehender Unreal-5.8-P8-Prototyp ist keine erneute Native-E2E-Abnahme dieses neuen Randfalltests.

Die aktualisierte P1-2D-Basisabnahme ist separat neu auszuführen, sobald die neue CMakeLists.txt-Quelle eingecheckt wird; andernfalls kann das konservative Gate wegen eines **korrekten SHA-Drifts** P1 vorübergehend als `INCOMPLETE` markieren.

## Reale Abnahme in dieser Iteration (MSVC Release, 26.10.2026)

- Build des neuen Testziels nach Initialisierung von `VsDevCmd.bat`: Exit 0. Ein erster Buildversuch **ohne** Visual-Studio-Developer-Umgebung scheiterte korrekt wegen fehlender Standard-C++-Header; der reparierte Environment-Aufruf war erfolgreich.
- Drei analytische 3D-Eigenmode-Raster: `8×7×6`, `10×9×8`, `12×11×10`, jeweils 3 adaptive Substeps.
- Maximale Abweichungen: `1.6653345369377348e-16`, `3.3306690738754696e-16`, `3.3306690738754696e-16`. Energieverhältnisse: `0.60942656650248073`, `0.76835995275905178`, `0.84629620082505064`.
- Native eigenmode Assertions: **9060**. Eigenständiges Python-Gate: **27/27 Checks PASS**, `partial_accepted=true`, `phase_complete=false`.
- Vollständige registrierte CTest-Suite: **39/39 PASS** (einschließlich D3D11-GPU-Tests), Exit 0.
- P1-2D-Referenz nach der CMake-Erweiterung neu gemessen: **33/33 Checks PASS**. Konservatives `phase_gate.py`: **P0/P1 PASS**, **P2–P8 INCOMPLETE**, `ALL_COMPLETE=False`.
