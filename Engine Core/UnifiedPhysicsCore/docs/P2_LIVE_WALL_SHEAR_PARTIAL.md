# P2: Live-MAC/VOF mit vorgegebener tangentialer Wandgeschwindigkeit (Teilabnahme)

Stand: 2026-10-26. **Nur abgegrenzter P2-CPU-Teilnachweis; keine Vollphasenabnahme.**

## Implementierter Umfang

`CoupledMacWater3D::set_wall_tangential_velocity({vx,vy,vz})` leitet eine
optionale, konstante Tangentialgeschwindigkeit einer **statischen**
Voxel-Festkörpergrenze an `ViscousMacDiffusion3D::step_masked` weiter.
Die Geometrie bewegt sich nicht; die Normalgeschwindigkeit bleibt null.
Default ist exakt `{0,0,0}`; bei `nu=0` bleibt der inviskose Pfad
unverändert. Ungültige nichtfinite Geschwindigkeiten werden vor einer
Zustandsänderung zurückgewiesen. Die Viskosität ist weiterhin explizit
mit CFL-begrenzten Substeps, einer freien Neumann-Oberflächen-Approximation
und einer stationären, voxelisierten Hindernismaske.

**Achtung:** Eine angetriebene Wand führt externe Arbeit zu; der
Face-Square-Energie-Proxy muss dann **nicht** monoton fallen.
Die bisherigen Dissipations-Assertions gelten nur für die ruhende Wand.

## Reproduzierbarer Nachweis

1. MSVC-Umgebung über `vcvars64.bat` aktivieren, Release-CMake-Build
   von `build` ausführen.
2. `build/upc_p2_live_viscous_water_tests.exe` prüft einen
   8×8×6-MAC/VOF-Tank mit statischer Festkörperwand x=2, tangentialer
   Wandgeschwindigkeit `vy=0.75 m/s`, `nu=0.035 m²/s`, 12 Zeitschritten
   `dt=0.02 s`. Gegenprobe: stationäre Wand und explizit
   `{0,0,0}` müssen zellgenau identisch sein. Die angetriebene
   Wand muss die Live-VOF-Entwicklung messbar ändern; Masse muss
   36 kg bleiben, kein Wasser darf in feste Zellen gelangen.
3. `build/upc_p2_viscosity_masked_tests.exe` prüft die unabhängige
   **diskrete** Couette-Einzelschritt-Referenz:
   `v_y=nu*dt/dx²*0.4 = 0.0144 m/s`, feste Normalgrenze = 0.
4. `scripts/p2_live_wall_shear_acceptance.py` wiederholt beide
   Binärtests, die vollständige CTest-Suite und `phase_gate.py`,
   schreibt Exitcodes, numerische Werte und SHA-256 der
   prüfrelevanten Quellen nach
   `ENGINE TEST STUBE/runs/p2_live_wall_shear_partial.json`.
5. `scripts/p2_live_viscosity_acceptance.py` aktualisiert den
   älteren Teilnachweis, der die neuen Live-Testfelder jetzt
   ebenfalls prüft.

## Messergebnisse und Grenzen

- MSVC Release-Build PASS; vollständige **35/35 CTest PASS**.
- Live-Test: **22.397 Assertions**, 12 Schritte mit vorgeschriebener
  Wandgeschwindigkeit, **36 kg Anfangs- und Endmasse**,
  maximale VOF-Differenz zur stationären Wand
  **0,230668223607722**. Explizit ruhende Wand und Default-Wand
  bleiben in allen geprüften Zellen exakt identisch.
- Maskierter Referenztest: **57 Assertions**,
  analytisch-diskreter Couette-Schritt **0,0144 m/s** gemessen
  und **0,0144 m/s** erwartet. Kein normaler Wanddurchfluss.
- Teilabnahme-Gate: **9/9 Checks PASS**, CTest-Exitcode 0,
  Gate-Exitcode 0, `GATES_INCOMPLETE=P2,P3,P4,P5,P6,P7,P8`,
  `ALL_COMPLETE=False`. Die ausführbaren Belege und SHA-256
  stehen im JSON. **`phase_complete=false` bleibt zwingend.**
- Nicht abgedeckt: bewegte oder deformierbare Festkörper, Impulsrückgabe
  an Körper, vollständige viskose Spannungstensoren und freie
  Oberflächen-Stressbedingungen, gemessene 3D-CFD-Referenzen,
  globale Energie-/Impulserhaltung und erneuter Unreal-5.8-Native-E2E
  für die neu hinzugefügte Wandgeschwindigkeit.
- **P2 bleibt PARTIAL, P3–P8 ebenfalls offen.** Kein Prototyp-Gate
  darf die Vollabnahme automatisch ersetzen.
