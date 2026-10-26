# P2 Teilnachweis: tangentiale Festkoerper-Wandgeschwindigkeit (26.10.2026)

**Status: PARTIAL. P2 ist NICHT vollstaendig abgenommen.**

## Umfang
Der maskierte explizite 3D-MAC-Viskositaetsoperator akzeptiert nun optional eine
vorgegebene, konstante **tangentiale Wandgeschwindigkeit** fuer stationaere
Festkoerpergeometrie (`wall_tangential_velocity_m_s`, 3 Komponenten).
Dies ist ein isolierter Couette-Scher-Randwert, **keine** dynamische
Festkoerperbewegung. Normale Solid-Faces bleiben undurchlaessig mit Geschwindigkeit 0;
Aussenwaende bleiben 0; Luft-Faces verwenden weiterhin den vorhandenen
Neumann-Nullgradient-Proxy. Ohne optionalen Parameter gilt die alte Nullwand-
Bedingung, und `nu=0` behaelt den bisherigen No-op-Pfad.

## Unabhaengige diskrete Referenz
10x10x10 MAC-Gitter, dx=0.1 m, nu=0.012 m2/s, dt=0.03 s.
Feste Solid-Ebene x<4, nasses Wassergebiet x>=4, initial alle Faces 0.
Vorgegebene Wandgeschwindigkeit (0.7, 0.4, 0) m/s; die x-Komponente\nsteht normal auf der Wand und wird im festen Geometriemodell verworfen.
Der benachbarte innere y-Face (4,5,5) hat genau einen Solid-Geisternachbarn
und fuenf ruhende Nachbarn. Fuer den expliziten 6-Punkt-Laplace-Schritt
gilt alpha=nu*dt/dx^2=0.036 und v_y=alpha*0.4=0.0144 m/s.
Test kontrolliert den Wert, eine ruhende Wand, Solid-Faces, normalen Nullfluss,
die nicht direkt benachbarte Zelle, das Verbot normaler\nGeschwindigkeitsinjektion und die Ablehnung von NaN-Wandwerten ohne
Zustandsveraenderung.

## Reale Tests und Reproduzierbarkeit
- MSVC Release Build der geaenderten und aller anderen Core-Targets: PASS.
- Volle native CTest-Suite: **35/35 PASS**, Exit 0 (26.10.2026).\n- Maskierter C++-Test: **57 Assertions**, darunter 0.0144 m/s\n  analytischer diskreter Wand-Scherwert und Nullfluss normal zur Wand.
- Das strukturierte quantitative Gate `scripts/p2_wall_shear_acceptance.py`
  fuehrt das C++-Testbinary sowie die komplette CTest-Suite erneut aus und
  schreibt `ENGINE TEST STUBE/runs/p2_wall_shear_partial.json`.
- Der JSON-Nachweis enthaelt reale Ausgaben, Pruefwerte und SHA256 aller
  relevanten Source-/Dokumentationsdateien. `scripts/phase_gate.py`
  liest den neuen Teilnachweis und belässt die P2-Vollphase auf PARTIAL.

## Explizite Grenzen
- Kein dynamisches Festkoerper-Mesh, keine Normalkomponente einer bewegten Wand.
- Keine Rueckwirkung der Scherkraft auf den Festkoerper, keine Impulsbilanz.
- Kein vollstaendiger spannungskorrekter Wasser-Luft-Rand, keine Oberflaechenspannung.
- Kein Vergleich mit gemessenen 3D-Referenzdaten; die Referenz ist ein
  **diskreter Einzelschritt**, keine Kontinuums- oder AAA-CFD-Abnahme.
- Kein Unreal-Native-E2E dieses neuen Wandrandwertes; P8 bleibt PARTIAL.
- Das Live-CoupledMacWater3D benutzt weiterhin den alten Default-Wandwert 0;
  der neue Parameter ist vorerst nur im isolierten maskierten Operator.
