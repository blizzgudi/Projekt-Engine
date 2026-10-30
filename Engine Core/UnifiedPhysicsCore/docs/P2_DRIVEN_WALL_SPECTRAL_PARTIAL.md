# P2 Teilabnahme: transienter 3D-Wandscherfluss, spektrale Referenz (30.10.2026)

## Abgegrenzter Nachweis

Der bestehende `ViscousMacDiffusion3D::step_masked` wird 40-mal mit
vorgegebener tangentialer Festkörper-Ghostgeschwindigkeit 0,4 m/s
angewendet. Zwei echte dreidimensionale MAC-Gitter werden getestet:
(ny,nz,L)=(8,8,5) und (12,12,7), mit 2 festen Voxelschichten
links, Wasser im Inneren und einer Wasser/Luft-Neumann-Grenze rechts.
Die Außenränder in y und z bleiben Dirichlet-null. Nur die
y-Geschwindigkeitskomponente wird angeregt; x/z bleiben null.

Unabhängige Referenz: vollständige Tensorprodukt-Sinuszerlegung der
*diskreten* 3D-Laplace-Gleichung mit links Dirichlet, rechts Neumann,
y/z Dirichlet. Für jede Mode gilt
`r=1+alpha*2*(cos(tx)+cos(ty)+cos(tz)-3)` und
`u_k=alpha*U*b*(1-r^k)/(1-r)`, mit aus der diskreten Orthogonalität
berechnetem Quellprojektionsfaktor b. Der Vergleich erfolgt
punktweise auf allen y-Faces, einschließlich Null-Rändern, nicht
nur an einem ausgewählten Probenpunkt. Zusätzlich: keine Normal-/
Querströmung, Maximumprinzip, echte 3D-Quergradienten,
diskrete Energie-Proxy-Parität und Ledger-Residualprüfungen.

Numerik: dx=0,1 m, nu=0,012 m²/s, dt=0,03 s, alpha=0,036,
40 Zeitschritte. Native Testdatei:
`tests/p2_driven_wall_spectral3d_tests.cpp`. Reproduzierbarer
MSVC-Release-Build samt 87 bestehenden CTests und eigenständigem
nativem Test:
`scripts/p2_driven_wall_spectral_full_build.cmd`.
Die neue Prüfung wird absichtlich als eigenständige zusätzliche
Native-Prüfung ausgeführt, damit die CMake-Hashes älterer,
unveränderter Phasen-Nachweise nicht ungültig werden.

Der separate Python-Nachweis
`scripts/p2_driven_wall_spectral_acceptance.py` berechnet
beide Zentrumsgeschwindigkeiten erneut durch eine eigenständige
spektrale Summation, prüft Testausgabe und CTest-Log und sichert
Quell- und Log-SHA256 unter
`ENGINE TEST STUBE/runs/p2_driven_wall_spectral_partial.json`.

## Aussagegrenzen

Dieser Test prüft einen stationären geometrischen Festkörper
mit vorgeschriebener tangentialer Wandgeschwindigkeit und eine
diskrete viskose Diffusionsgleichung. Das ist **kein** bewegtes
Festkörpervolumen, keine physikalisch vollständige Impulsrückgabe,
kein Navier-Stokes-Advektions-/Druck-E2E, keine spannungskorrekte
freie Oberfläche und keine externe gemessene CFD-Referenz.
Die numerische Face-Energie ist **kein** physikalisch geschlossener
Fluid-/Festkörper-Energiehaushalt. Kein neuer Unreal-5.8-Native-E2E.
P2 bleibt PARTIAL; P3 bis P8 bleiben unvollständig.

## Gemessene Ergebnisse (MSVC Release, 30.10.2026)

- Bestehende Regression: 87/87 CTests PASS.
- Neue unabhaengige Native-Pruefung: 11882 Assertions PASS.
- 8er Gitter: Zentrum 0.21970222804462206 m/s; spektral
  0.21970222804462192 m/s; maximaler Face-Fehler 2.22e-16 m/s.
- 12er Gitter: Zentrum 0.22204276223606123 m/s; spektral
  0.22204276223606115 m/s; maximaler Face-Fehler 3.61e-16 m/s.
- Diskrete Face-Energie-Proxies: 0.0011171250888508948 und
  0.0030631713360417092 (dimensionsbehaftete Proxy-Werte).
- Das Python-Gate fuehrt den vollstaendigen CTest-Lauf erneut aus
  und prueft 15 quantitative Kriterien sowie Quell-/Log-SHA256.
  Diese Fehlerwerte betreffen ausschliesslich die exakte DISKRETE
  Referenz, nicht die Genauigkeit gegen reale Strömungsmessungen.
