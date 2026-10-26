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


## Erweiterung: kontinuierliche 3D-PDE-Referenz mit Rasterverfeinerung (26.10.2026)

Der vorherige Beleg prüfte ausschließlich die **diskrete** Eigenmode. Jetzt
wird zusätzlich eine **kontinuierliche, analytisch bekannte Lösung** der
dreidimensionalen viskosen Diffusionsgleichung geprüft:

```text
∂u_y/∂t = ν (∂²u_y/∂x² + ∂²u_y/∂y² + ∂²u_y/∂z²)
u_y(x,y,z,0) = sin(kx*x) sin(ky*y) sin(kz*z)
u_y(x,y,z,T) = u_y(x,y,z,0) exp[-ν T (kx²+ky²+kz²)]
```

Das physikalische x-Intervall ist **1 m** lang. Links gilt feste
Dirichlet-Ghost-Wand `u=0`, rechts Neumann-Ghost `∂u/∂x=0`,
in y und z Dirichlet-Außenwände. Die Gitterkoordinaten sind
`x=(j+1)dx`, `y=j dx`, `z=(j+1)dx`, mit
`dx=1/(L+0.5)`, `kx=π/((2L+1)dx)`,
`ky=π/(ny dx)`, `kz=π/((nz+1)dx)`. Dies ist eine
Manufactured-Solution für den **stationär maskierten 3D-MAC-Diffusionsoperator**,
nicht für die volle 3D-Navier-Stokes-Gleichung.

Vier Raster `L=5,9,17,33` werden bei `ν=0.01 m²/s`,
`T=0.04 s`, **32 Zeitschritten** verglichen. Der native Test prüft
alle y-Faces gegen die separate diskrete Eigenformel und die gemessene
Maximalabweichung zur **kontinuierlichen** Formel. Er kontrolliert in jedem
Zeitschritt die diskrete Energie-/Face-Impuls-Summenbilanz. Das Python-Gate
rekonstruiert unabhängig beide Faktoren und prüft fallende Fehler über
alle vier Raster.

| Raster (nx×ny×nz) | dx [m] | max. Abweichung zur kontinuierlichen PDE | max. Face-Ledger-Residuum |
|---|---:|---:|---:|
| 10×6×5 | 0.18181818 | 1.5326799830905902e-4 | 6.3069341416088776e-14 |
| 14×10×9 | 0.10526316 | 5.8966339550714508e-5 | 8.5211004918761546e-13 |
| 22×18×17 | 0.05714286 | 1.8303755349946549e-5 | 1.9930612715768348e-11 |
| 38×34×33 | 0.02985075 | 4.4248190426143807e-6 | 2.4627544448208027e-10 |

Der Fehler sinkt vom grobsten zum feinsten Raster um etwa den Faktor
**34.6**. Der Grenzwert für die unskalierte Face-Summenbilanz ist
`1e-8` (Rundung bei vielen Tausend Faces); dies ist **kein**
physikalisches Energieerhaltungsversprechen.

Der erweiterte Test erfasst **62.319 native Assertions** und
`phase_complete=false`. Die quantitative, unabhängige
Python-Teilabnahme und die vollständige 39-Test-CTest-Suite sind
zusätzlich erforderlich, bevor `partial_accepted=true` gelten darf.

**Nicht abgenommen:** reale gemessene 3D-CFD-Daten, stresskonsistente
Wasser-Luft-Grenzspannung, bewegte Körper, bidirektionale
Festkörper-Impulse, globale Energie-/Impulsbilanz, integrierte GPU-/LOD-
und Unreal-5.8-Native-E2E-Gesamtphase. **P2 bleibt PARTIAL.**


### Abnahmeprotokoll der Erweiterung

- VS BuildTools 18 / MSVC Release: vollständiger CMake-Build Exit **0**.
- Gesamte native CTest-Suite: **39/39 PASS**, Exit **0**.
  Protokoll: `ENGINE TEST STUBE/runs/p2_continuum_refinement_ctest_20261026.txt`,
  UTF-8 SHA-256 `81f0f3125ff40b89bccd28c07e028f8e5acd6ac9c71c9ffe88e88c3fa3413e8f`.
- Erweitertes natives 3D-MAC-Testprogramm: **62.319 Assertions PASS**,
  einschließlich vier PDE-Rasterverfeinerungen und 32 Zeitschritten.
- Unabhängiges Python-Gate: **59/59 quantitative Checks PASS**,
  `partial_accepted=true`, `phase_complete=false`.
- Erneuter konservativer Gesamt-Gate-Lauf nach dem GPU-Einzeltest:
  **P0/P1 PASS**, **P2–P8 PARTIAL**, `ALL_COMPLETE=False`.
- Der erste Gesamtlauf zeigte einen sporadischen D3D11-P7-`bad allocation`-
  Fehler. Der unmittelbar darauf einzeln wiederholte reale GPU-Test und die
  neue vollständige 39er-Suite bestanden. Dies ist **kein Nachweis
  deterministischer GPU-Stabilität**; Langzeitspeicher-/GPU-Lasttest offen.
- Das erste MSVC-Build dieser Erweiterung ohne `VsDevCmd.bat` scheiterte
  an fehlenden C++-Standardheadern; nach Initialisierung der vorhandenen
  Developer-Umgebung wurde derselbe Quellstand erfolgreich gebaut.

Diese Zahlen beziehen sich nur auf diesen lokalen Lauf und beweisen
**keinen** Vollphasenabschluss P2 oder P7/P8.
