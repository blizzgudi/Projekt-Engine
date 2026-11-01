# Projekt Engine — Unified Physics Core / Phasen

Stand: 2026-10-26. Status bezieht sich auf den realen Code, nicht auf Ambitionen.

| Phase | Inhalt | Akzeptanzkriterium | Stand |
|---|---|---|---|
| P0 | Repo/Ordnerstruktur, C++17-Build, Material-IDs, 3D-Voxelzustand, Tests | MSVC kompiliert, Tests gruen, Git commit | MSVC-Build und 5 CTest-Suites PASS, lokaler Git-Commit vorhanden |
| P1 | Konservativer Basis-Wassersolver als Nachweis | Dammbruch, Druck, stabile Masse, Hindernisse | **PASS (2D CPU-MVP)**: verbesserter HLL-Fluss, unabhängige Ritter-Trockenbett- und Stoker-Nassbett-Analytik mit 3 Gitterauflösungen, hydrostatischer Druck, 2D-Symmetrie, Tor/Hindernisse, positive Wasserhöhe, CFL, Massenbilanz; isolinienabhängige Frontabweichung dokumentiert (docs/P1_2D_FLUID_ABNAHME.md) |
| P2 | Echte volumetrische 3D-Wassertechnik erforschen und auswaehlen (MAC/FLIP/APIC/MPM) | Vergleich mit Referenzfaellen, Speicherbudget, 3D-Kollision | **PROTOTYP-PASS / VOLL-PHASE OFFEN (CPU-MVP)**: MAC+VOF ausgewählt, gekoppelter 3D-Solver, Referenz-Hydrostatik, 3D-Dammbruch/Hindernis, Auflösungs-/CPU-/Speichervergleich und gemeinsame Core-API geprüft; Einschränkungen siehe docs/P2_3D_FLUID_ABNAHME.md |
| P3 | Gekoppelte Material- und Volumenphysik | Wasser sickert in poroesen Boden, Krafteinwirkung, Energiebilanz | **PROTOTYP-PASS / VOLL-PHASE OFFEN (abgegrenzter 3D-CPU-MVP)**: zentrales Material-/Voxel-/MAC-System koppelt 3D-Wasser und vertikal infiltrierenden Boden, konserviert Wassermasse, bilanziert Gravitationsenergie-Dissipation, berechnet Druckkraft, bewegt rigides 1-Voxel-Objekt und verdrängt Wasser ohne Volumenverlust; 45 quantitative Gates und 505 native Assertions; Kontinuums-FSI/gesamte Fluidenergie bleiben nicht validiert (docs/P3_MATERIAL_VOLUME_ABNAHME.md) |
| P4 | Thermodynamik und Phasenwechsel | validierte Waerme-/Massenerhaltung; Wasser/Eis/Dampf | **PROTOTYP-PASS / VOLL-PHASE OFFEN (3D-CPU-MVP)**: 3D-Fourier-Leitung und analytische Neumann-Vergleiche (4 Raster), Eis/Wasser/Dampf mit Enthalpieplateaus, dynamische MAC-Massen-/Enthalpie-Advektion, Voxel-Eis-Kollisionsmaske und Wiederverflüssigung, Dampf-Inventar, gas-/eis-volumetrische Zustandsdiagnose sowie P3-Darcy-Gravitationsenergie zu Porenwärme; keine echte Gasströmung, keine vollständig gekoppelte mechanische Gesamtenergie (docs/P4_THERMAL_CPU_MVP_ABNAHME.md). |
| P5 | Festkoerper, Bruch, granulare Stoffe | Belastungs-/Bruchtests und stabile Kontakte | **PROTOTYP-PASS / VOLL-PHASE OFFEN (abgegrenzter 3D-CPU-MVP)**: 26-Nachbar-Volumen-Federgitter mit Dehnungsbruch, zwei nachgewiesenen Fragmenten und VoxelGrid-Damage-Writeback; robuste feste Ebenenkontakte, Paar-/Wandkornkollisionen, Schwerkraftstapel; nichtgebundene volumetrische Knotenselbst- und Mehrkörperkontakte inklusive Impuls-/Bruchdissipationsprüfung; Hooke/e²/affine Zug- und Scher-Referenz bei 4³/6³/8³. **Kein** FEM/MPM-Kontinuum, exakt verformte Meshoberflächenkontakte, unabhängige Sandmessdaten oder globale FSI-Energievalidierung (docs/P5_SOLID_GRANULAR_CPU_MVP_ABNAHME.md). |
| P6 | Dynamische Materialmischungen und Regeln | Mischung mit Massenbilanz, reproduzierbare Reaktionen | **PROTOTYP-PASS / VOLL-PHASE OFFEN (abgegrenzter 3D-CPU-MVP)**: MaterialLibrary-ID-basierte mehrspeziesige Voxel-Zusammensetzung (mol, Masse, Dichte/Volumen), Element- und Massenbilanzprüfung an jeder Reaktionsdefinition, begrenzte Arrhenius-Reaktionsregeln, chemische-zu-sensible Reaktionswärme, konservative sechsfächige 3D-Fick-Diffusion, dominante Material-ID als VoxelGrid-Proxy und energieerhaltender Kontakt zum P4-Thermal-Solver; quantitative 285/320/380 K Kinetikreferenz und 8³/12³/16³-Diffusionsmessungen. **Keine** automatisch kalibrierte reale Chemie, Multiphase-CFD, universelle Materialreaktion oder globale FSI-Energievalidierung (docs/P6_REACTIVE_MATERIALS_CPU_MVP_ABNAHME.md). |
| P7 | Sparse Chunks, LOD, GPU-Prototyp | CPU/GPU-Vergleich, Skalierung, Latenzgrenzen | **PROTOTYP-PASS / VOLL-PHASE OFFEN (abgegrenzter Hardware-GPU-3D-Render-LOD-MVP)**: reale NVIDIA-Hardware-D3D11-Compute-Shader-LOD-Reduktion für Wasser/Solid (20 GPU-Dispatches auf 17×13×9 / P8-MAC 12×8×6 / 32³ / 48³ / 64³ und Skalen 2/4/8/16), CPU-Sparse-LOD-Zellparität und Massenbilanz; gemessene GPU-Kernelzeit und CPU↔GPU-Roundtrip-Latenzen/GPU-Budget, Kamera-LOD-Streaming mit 8³-Sparse-Bricks/Generationen und festem Frame-Budget; 3 echte D3D11-VRAM-Uploads mit bytegenauem GPU-Readback. UE5.8-Game+PIE P8 erneut als bestehende Native-Referenz geprüft. *Kein* GPU-Fluiddrucksolver, UE-RDG-Anbindung/AAA-60-FPS oder nachgewiesener kompletter GPU-Speedup. Siehe docs/P7_GPU_LOD_STREAMING_CPU_GPU_ABNAHME.md. |
| P8 | UE5.8-Anbindung ueber genau einen Adapter | Editor-Reload, Runtime-Test, Rendering/E2E | **PROTOTYP-PASS / VOLL-PHASE OFFEN (abgegrenzter UE5.8-Native-E2E für P2-3D-MAC-Wasser)**: unabhängiges Runtime-Plugin EngineMaterialLab nutzt reale Standalone-UPC-C++-Header mit unverändertem SHA; Win64-Editor-Development/Game-Development/Game-Shipping-BuildPlugin erfolgreich. Zwei separate echte UnrealEditor-Prozesse: Game und editorinterne PIE-Simulation, beide ExitCode 0 ohne Crash. 12×8×6 3D-MAC-Flüssigkeitsbewegung, konstante 128 kg, Wasserausbreitung außerhalb des ursprünglichen Damms und 280 UE-InstancedStaticMesh-Wasservoxel; echte GPU-SceneCapture-PNG 960×540 in Game UND PIE sowie überprüfte Logs/Pixel/Hashes. P3–P6 noch nicht als UE-Szenen integriert, P7-Hardware-GPU-Prototyp ist separat validiert, aber nicht in Unreal integriert (docs/P8_UNREAL58_NATIVE_E2E_ABNAHME.md). |

## Architekturregeln
1. D:\\Projekt Engine ist Projektwurzel. Keine neuen Root-Ordner auf D:\\.
2. Eigenstaendige C++-Bibliothek. Unreal ist Verbraucher, keine Pflichtabhaengigkeit.
3. Gemeinsame SI-Einheiten, Material-IDs, vereinbarte Phasen und Zeitsteuerung.
4. Wenige Methoden intern, aber keine universelle Wunderformel fuer jede Materialphase.
5. Tests pruefen Erhaltung, Grenzwerte, Stabilitaet, Referenzfaelle und reale Laufzeit.
6. Nie aus einem 2D-Wassertest auf korrektes 3D-Wasser, Boden oder AAA-Spielqualitaet schliessen.
7. GitHub: privates separates Repo \`Projekt-Engine\` bevorzugt; GitHub-Remote erst nach tatsaechlich bestaetigter Erstellung eintragen. Bestehendes AI-Game-Studio-Repo nicht veraendern.

## Aktuelle Beleglage
- MSVC-CMake Release: 5/5 Tests erfolgreich; erweiterte Material-/Fluid-/Solid-Tests nachträglich ergänzt.
- Unreal Engine 5.8: echter RunUAT BuildPlugin Win64 UnrealEditor-Development, UnrealGame-Development und UnrealGame-Shipping PASS; reale Native-Game-/Editor-PIE-E2E samt ExitCode 0, Live-3D-MAC-VOF-Bewegung und gerenderten PNGs durch P8-Abnahmeskript nachgewiesen.
- CPU-Benchmark: `ENGINE TEST STUBE/runs/unified_physics_v0_2_cpu_bench.csv`.
- Maschinenlesbare Statuspruefung: `Engine Core/UnifiedPhysicsCore/scripts/phase_gate.py`.
- Externe Grundlagen: Robert Bridson, SIGGRAPH Fluids 2007 (MAC/Projection); UE5.8 Plugin/Module API Dokumentation.
- P0 bis P8 gelten **nur innerhalb der einzeln definierten CPU-/GPU-/UE-Prototyp-Abnahmen** als abgeschlossen; reale AAA-Engine-Produktion und ganzheitliche FSI/GPU-Physik sind ausdrücklich nicht nachgewiesen; P2 ist kein vollstaendig validiertes 3D-CFD- oder AAA-Fluidprodukt.
- P2-Pruefnachweis: ENGINE TEST STUBE/runs/p2_acceptance.json (native quantitative Checks + Hashes), Script: scripts/p2_acceptance.py.

- P1-Pruefnachweis: ENGINE TEST STUBE/runs/p1_acceptance.json, scripts/p1_acceptance.py, analytische Referenzen Ritter/Stoker, native Tests und SHA256-Fingerprints.

- P3-Nachweis: ENGINE TEST STUBE/runs/p3_acceptance.json (45 Gates mit 3 Darcy-Gittern, Porenbilanz, Druckkraft, Rigid-Voxel-Remap, 3D-Performance) sowie tests/material_coupling3d_tests.cpp und scripts/p3_acceptance.py. Vollständige dynamische Mehrkörper-FSI ist kein belegtes Ergebnis.

- P4-Teilnachweis (NICHT P4-PASS): ENGINE TEST STUBE/runs/p4_partial_acceptance.json; native tests/thermal3d_tests.cpp, analytische benchmarks/p4_reference.cpp und scripts/p4_partial_acceptance.py. Live-Fluid-Phasenwechsel weiterhin offen.

- P4-Vollnachweis (CPU-MVP): ENGINE TEST STUBE/runs/p4_acceptance.json, scripts/p4_acceptance.py. Vorheriger Teilnachweis bleibt historisch nachvollziehbar. Live-MAC/Enthalpie, 3D-Phasen, poröses P3-Wärmekonto überprüft.

- P5: ENGINE TEST STUBE/runs/p5_acceptance.json; scripts/p5_acceptance.py, zusätzlich p5_contact_partial.json historisch. Granular/Lattice/Bruch-Demonstrator sind keine universelle Kontinuums-Festkörper-/DEM-/MPM-Validierung.

- P5-Teilnachweis (P5 bleibt PARTIAL): ENGINE TEST STUBE/runs/p5_acceptance.json; 3D-Lattice/Fracture, granular contacts, quantitative benchmarks, Voxel-Damage; scripts/p5_acceptance.py. Keine vollständige Phasenabnahme.

- P5-Gesamtvalidierung: ENGINE TEST STUBE/runs/p5_full_acceptance.json, scripts/p5_full_acceptance.py; ältere p5_acceptance.json und p5_contact_partial.json bleiben als **Teilmeilensteine** erhalten. Full-Gate, Source-SHA256 und alle CTest-Gruppen erforderlich.

- P6-Abnahme: ENGINE TEST STUBE/runs/p6_acceptance.json und scripts/p6_acceptance.py (synthetische, bilanzierte 3D-Reaktionen, numerischer Fick/Arrhenius-Test, P4-Konservierungskontakt). Externe Stoffdatenbank/realchemische Vorhersage weiterhin offen.

- P7-Teilnachweis: ENGINE TEST STUBE/runs/p7_sparse_partial.json; scripts/p7_sparse_partial_acceptance.py. Historischer Teilnachweis; keine GPU-Aussage allein aus diesem Report.

- P8 Native UE5.8: ENGINE TEST STUBE/runs/p8_acceptance.json, scripts/p8_acceptance.py und ENGINE TEST STUBE/scripts/p8_native_e2e.py. Runtime-PNG+Game/PIE-Protokolle mit SHA256. P3–P6 sind nicht im UE-Renderer der P8-Demonstration enthalten.

- P7 Gesamtnachweis: ENGINE TEST STUBE/runs/p7_acceptance.json, scripts/p7_acceptance.py; echte Direct3D11-GPU mit CPU-Parität, 8³-Kachel-Streaming, framegebundenem GPU-VRAM-Upload, Shader-Quellhash, P8 Game/PIE als Vorbeweis. Die ältere P7_SPARSE_LOD_PARTIAL.md bleibt als historischer Teilnachweis erhalten.

## Verbindlicher Vollphasenstatus und nächste Arbeiten

Die früheren grünen MVP-Gates belegen **nur den dokumentierten Testumfang**.
Sie dürfen nicht als vollständige Entwicklungsphasen oder als fertige
physikalisch korrekte Engine interpretiert werden. Die Gate-Datei
`scripts/phase_gate.py` meldet deshalb getrennt `prototype_status`,
`status` und `full_acceptance_open`. P0/P1 sind innerhalb ihres
Basisumfangs abgenommen; P2–P8 bleiben **PARTIAL** bis alle
unabhängigen Vollkriterien erfüllt und nachgewiesen sind.
`ALL_COMPLETE=True` ist bei bloßen MVP-PASS-Nachweisen verboten.

Nächste tatsächlich offene Phase: **P2 – vollständige 3D-Flüssigkeitsvalidierung**.
Fortschritt am 26.10.2026: optionaler 3D-MAC-Viskositätsoperator,
1174 native Assertions, unabhängiger diskreter Eigenmode-Test
(Maximalabweichung 2.22e-16), dissipierender Energie-Proxy,
adaptives Viskositäts-CFL, 33/33 CTest-Suites. Siehe
`docs/P2_VISCOUS_MAC_PARTIAL.md` und
`ENGINE TEST STUBE/runs/p2_viscosity_partial.json`.
Weiterer Teilfortschritt am 26.10.2026: statische 3D-Wasser-/Feststoff-
Masken für den MAC-Viskositätsoperator, freie Oberflächen als
Neumann-Nullgradient-Proxy, stationäre Festkörper und Außenwände
mit Dirichlet-Null-Geschwindigkeit, eigenständige native Testfälle
mit analytischem Wasser-Luft-Diskretisierungsstencil, 34/34 CTest.
Keine Kopplung in den Live-Solver und keine Vollabnahme.
**Noch offen:** Viskosität in `CoupledMacWater3D` mit
Wasser-Luft-/Festkörper-Randbedingungen integrieren,
3D-Referenzexperimente, Impuls-/Energiebilanz und
bewegliche Körper. Der isolierte Operator schließt P2 nicht ab.

Weitere Vollphasen-Lücken sind im Feld `full_acceptance_open`
des maschinenlesbaren Gates pro P3–P8 aufgelistet. Vor
jeder echten Vollphasenfreigabe müssen diese durch separate
reproduzierbare Abnahmeskripte mit unabhängigen Referenzen,
Hashnachweisen und echten Native-E2E-Tests geschlossen werden.

## Aktueller P2-Teilfortschritt: Live-Viskosität im gekoppelten MAC/VOF (26.10.2026)

- Opt-in `set_kinematic_viscosity(nu_m2_s)` in
  `include/upc/coupled_water3d.hpp`. Bei `nu=0` bleibt die
  bestehende inviskose Wasser-Implementierung bitgleich.
- Echte Kopplung von `ViscousMacDiffusion3D::step_masked` mit
  dynamischer Wet-Maske, statischen Solid-Hindernissen, Live-VOF-
  Transport und nachfolgender Druckprojektion.
- MSVC Release: 35/35 CTest PASS. Separater Native-Test
  `tests/coupled_viscous_water3d_tests.cpp`: 12.553 Assertions,
  90 kg Wasser -> 90 kg, 36 interne Viskositäts-Substeps,
  Face-Energie-Proxy-Abnahme 0,0119468516707611 und
  VOF-Unterschied zum inviskosen Vergleich 0,121149145596311.
- Unabhängiges Teil-Gate:
  `scripts/p2_live_viscosity_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_live_viscosity_partial.json`.
  Vollständige Belege und Grenzen: `docs/P2_LIVE_VISCOUS_MAC_PARTIAL.md`.
- **P2 bleibt PARTIAL**: spannungskorrekte Wasser-Luft-Grenze,
  bewegte Festkörper mit Impulsrückgabe, unabhängige gemessene
  3D-Strömungsreferenzen und globale Energie-/Impulsvalidierung offen.
  P3–P8 ebenfalls nicht voll abgenommen.
- **Aktualisierte P8-Native-E2E-Probe:** isoliertes UE5.8-Plugin mit
  aktiviertem viskosen P2-Pfad neu gebaut (Win64 Editor/Game/Shipping),
  zwei echte Game-/PIE-Runs Exit 0 mit 111 Viskositäts-Substeps und
  realen 960×540 Render-PNGs. `scripts/p8_acceptance.py`:
  23/23 Checks PASS. P8-Vollphase bleibt **PARTIAL**.

## P2 Teilfortschritt: vorgeschriebene tangentiale Wandgeschwindigkeit (26.10.2026)

- `ViscousMacDiffusion3D::step_masked` unterstuetzt einen optionalen
  konstanten tangentialen Festkoerper-Wandwert (Couette-Scherbedingung,
  unveraenderte Geometrie, keine Normalbewegung). Die Default-Wand ist
  weiterhin stationaer; der `nu=0`-Pfad bleibt unveraendert.
- Native MSVC Release Build und vollstaendige 35/35 CTest-Suite PASS.
  57 Assertions im maskierten Native-Test, diskrete Einzelschritt-Referenz
  `v_y=nu*dt/dx^2*0.4=0.0144 m/s` exakt innerhalb 1e-13.
  Negativtest fuer nichtfinite Wandgeschwindigkeit, kein Normalfluss.
- Separates maschinenlesbares Gate:
  `scripts/p2_wall_shear_acceptance.py` und
  `ENGINE TEST STUBE/runs/p2_wall_shear_partial.json`.
  Dokumentation: `docs/P2_WALL_SHEAR_PARTIAL.md`.
- **P2 bleibt PARTIAL.** Kein bewegtes Festkoerper-Mesh, keine
  Wandimpuls-Rueckwirkung, keine spannungskorrekte freie Oberflaeche,
  keine unabhaengigen 3D-Messdaten oder neuer UE5.8-Native-E2E-Test.
  Der Live-Solver nutzt vorerst weiterhin die Default-Wand 0.

## P2 Teilfortschritt: Live-Couette-Scherung an statischen Voxelwänden (26.10.2026)

- Optionales `set_wall_tangential_velocity({vx,vy,vz})` in
  `CoupledMacWater3D`: konstante vorgeschriebene tangentiale
  Festkörper-Wandgeschwindigkeit wird im **laufenden** 3D-MAC/VOF-
  Viskositätsoperator angewendet; Default null, inviskoser Pfad unverändert.
- Native MSVC Release: **35/35 CTest PASS**; Live-Test mit
  **22.397 Assertions**, 36 kg Wassermasse exakt erhalten,
  VOF-Differenz zur stationären Wand 0,230668223607722;
  unabhängiger diskreter Couette-Einzelschritt 0,0144 m/s (57 Checks).
- Maschinenlesbare Teilabnahme:
  `scripts/p2_live_wall_shear_acceptance.py` ->
  `ENGINE TEST STUBE/runs/p2_live_wall_shear_partial.json`,
  9/9 Checks PASS mit Source-SHA256, `phase_complete=false`.
  Dokumentation: `docs/P2_LIVE_WALL_SHEAR_PARTIAL.md`.
- **P2 weiterhin PARTIAL**: Es ist eine statische Geometrie mit
  vorgegebenem Wandgeschwindigkeits-Ghost, **kein** bewegter Festkörper,
  keine Impulsrückwirkung, keine stresskonsistente freie Oberfläche,
  keine unabhängigen gemessenen 3D-Referenzen oder erneuter UE5.8
  Native-E2E-Nachweis für diesen neuen Parameter. P3–P8 bleiben offen.


## P2 Teilfortschritt: Maskierte 3D-MAC-Wand-Impulsdiagnose (26.10.2026)

- Der laufende 3D-MAC/VOF-Solver erfasst optional die **diskrete
  Face-Geschwindigkeitssummenbilanz** des Viskosit?tsoperators getrennt
  nach statischer Festk?rperwand und ?u?erer Nullgeschwindigkeitsgrenze.
  Live-Akkumulation pro Solver-Step und R?cksetzen bei `nu=0`.
- Unabh?ngige Couette-Stencil-Referenz: 90 angrenzende tangentiale
  Faces ? 0,036 ? 0,4 m/s = **1,296 m/s**. Der neue native Test
  pr?ft zus?tzlich Vorzeichenwechsel, Normalfluss, 8 adaptive
  Substeps, Negativfall und Live-36-kg-VOF-Massenkonstanz.
- MSVC Release **36/36 CTest PASS**, 53 native Assertions im neuen
  Test, Abschlussresiduum des diskreten Face-Summen-Operators 0
  im protokollierten Live-Fall.
- Nachweis: `docs/P2_VISCOUS_MOMENTUM_LEDGER_PARTIAL.md`,
  `scripts/p2_momentum_ledger_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_viscous_momentum_ledger_partial.json`
  mit Quell-SHA256 und `phase_complete=false`.
- **Wichtig:** Die als `*_proxy_kg_m_s` ausgegebene Summe ist
  **keine physikalische Wandimpuls-/FSI-Bilanz**. Es fehlen echte
  bewegte K?rper, stresskonsistente freie Oberfl?chen, unabh?ngige
  3D-Messreferenzen, globale Energie-/Impulsbilanz und neue UE5.8
  Native-E2E-Abnahme. **P2 bleibt PARTIAL, P3?P8 bleiben offen.**

## P2 Teilfortschritt: Zerlegung der statischen MAC-Wandquelle (26.10.2026)

- `ViscousMacDiffusion3D::step_masked` trennt die diskrete
  Festkörper-Grenzquelle in `alpha*v_ghost` (vorgegebener Drive)
  und `-alpha*v_face` (lokaler Drag). Beide sind ausdrücklich
  **nur numerische staggered-Face-Summen-Proxies**.
- `CoupledMacWater3D` akkumuliert beide Beiträge über die
  tatsächlichen Live-Viskositäts-Substeps, mit Reset bei `nu=0`.
- Unabhängige diskrete Couette-Referenz: 90 Wand-Faces,
  `alpha=0.036`, `v_wall=0.4 m/s` -> Drive `1.296 m/s`.
  Ein vorbelegtes Face mit `v=0.25 m/s` ergibt Drag `-0.009 m/s`.
- Nachweis: `docs/P2_WALL_DRIVE_DRAG_PARTIAL.md`,
  `scripts/p2_wall_drive_drag_acceptance.py` und
  `ENGINE TEST STUBE/runs/p2_wall_drive_drag_partial.json`.
  Der Report gilt nur bei echtem `partial_accepted=true`.
- **P2 bleibt PARTIAL**, ebenso P3–P8. Keine bewegte Körpergeometrie,
  keine echte FSI-Gegenreaktion, keine stresskonsistente freie
  Oberfläche, keine externen 3D-Messdaten oder neue UE5.8-Native-E2E.


## P2 Teilfortschritt: exakte diskrete MAC-Face-Energiebilanz (26.10.2026)

- `MaskedMacMomentumLedger3D` bilanziert jetzt zusätzlich die Änderung
  der aktiven staggered-Face-Quadratsumme als **numerischen Proxy**:
  interne Gradientenverluste, statische Solid-/Außenwand-Verluste,
  vorgegebene tangentiale Wandarbeit und positive Euler-Korrektur.
  Die Summe schließt algebraisch gegen die tatsächliche Face-Änderung.
- Unabhängige 10³-Couette-Einzelschritt-Referenz bei `alpha=0,036`:
  `internal_loss=0,01125`, `solid_loss=0,00225`,
  `wall_work=0,0036`, `Euler_correction=0,0107325`,
  `face_energy_delta=+0,0008325`. Die positive Änderung bei
  angetriebener Wand ist ausdrücklich **kein** FSI-Energiebeweis.
- Native Test `tests/viscous_energy_ledger3d_tests.cpp` prüft
  die Referenz, 3 Achsen, stationäre Wand, freie Oberflächen,
  adaptive 8-Substeps, Nullviskosität und Negativfall.
  MSVC Release **37/37 CTest PASS**.
- Gate: `scripts/p2_viscous_energy_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_viscous_energy_partial.json`.
  Dokumentation: `docs/P2_VISCOUS_ENERGY_LEDGER_PARTIAL.md`.
- **P2 bleibt PARTIAL, P3–P8 bleiben offen.** Es fehlt die
  physikalische Gesamtenergie/Impulsbilanz, bewegte Festkörper-FSI,
  stresskonsistente Wasser-Luft-Grenze, unabhängige 3D-Messreferenz
  und ein neuer UE5.8-Native-E2E-Lauf für diese Erweiterung.

## P2 Teilfortschritt: Live-VOF-Aggregation des diskreten Viskositaets-Energieledgers (26.10.2026)

- `CoupledMacWater3D::last_viscous_diffusion_ledger()` akkumuliert die
  isoliert geprüften diskreten MAC-Face-Quadratsummen-Bilanzterme ueber
  alle adaptiven Viskositaetsstufen eines echten 3D-MAC/VOF-`step()`.
  Nach jedem oeffentlichen Step erfolgt Reset, auch fuer `nu=0`.
- Neue native 8x8x6-Wasser/Festkoerper-Tests: 697 Assertions PASS,
  36 kg Wasser, 24 Viskositaets-Substeps in 10 Live-Steps,
  vorgegebene Wandarbeit 2.8622854929466457 und Gradientverlust
  2.437429725141957 (jeweils **numerische Face-Proxies**, keine Joule).
- Dokumentation: `docs/P2_LIVE_ENERGY_LEDGER_PARTIAL.md`;
  Test: `tests/coupled_viscous_energy_ledger3d_tests.cpp`;
  Teil-Gate: `scripts/p2_live_energy_acceptance.py`;
  Report: `ENGINE TEST STUBE/runs/p2_live_energy_partial.json`.
- **P2 bleibt PARTIAL**: keine bewegliche FSI-Koerpergeometrie,
  keine physikalische globale Energie-/Impulsbilanz, keine gemessene
  3D-CFD-Referenz, keine neue UE5.8-Native-E2E-Abnahme.
  P3-P8 bleiben ebenfalls Vollphasen-offen.

## P2 Teilfortschritt: unabhängiger diskreter 3D-MAC-Mischrand-Eigenmode (26.10.2026)

- Drei maskierte 3D-Gitter `8×7×6`, `10×9×8`, `12×11×10` mit fester Dirichlet-Festkörperwand links, Neumann-Nullgradient-Proxy an der Wasser/Luft-Grenze rechts, äußerem No-Slip und analytisch separierter x/y/z-Eigenfunktion. Drei adaptive Viskositäts-Substeps pro Gitter.
- Neuer MSVC-Native-Test `tests/viscous_masked_eigenmode3d_tests.cpp`: **9060 Assertions PASS**, maximale diskrete Eigenmode-Abweichung `3.33e-16`; unabhängige Prüfung der exakten Face-Energieverhältnisse und eines vorgeschriebenen Couette-Ghost-Wandscher-Einzelschritts.
- Separates Gate `scripts/p2_masked_mixed_bc_acceptance.py`: **27/27 PASS**, `phase_complete=false`, Hashnachweise unter `ENGINE TEST STUBE/runs/p2_masked_mixed_bc_partial.json`.
- Komplette native CTest-Suite **39/39 PASS**; nach Änderung von `CMakeLists.txt` wurde P1-2D mit **33/33 PASS** und aktualisierten Quellhashes reproduziert. `phase_gate.py`: **P0,P1 PASS; P2–P8 INCOMPLETE; ALL_COMPLETE=False**.
- Grenzen: diskreter Eigenmode ist **keine externe Messreferenz**, keine vollständige freie-Oberflächen-Spannungsrandbedingung, keine bewegte Körper-FSI oder globale Energie-/Impulsabnahme. P2 bleibt **PARTIAL**. Details: `docs/P2_MASKED_MIXED_BC_EIGENMODE_PARTIAL.md`.

## P2 Teilfortschritt: 3D-MAC-Druckprojektion gegen Manufactured Pressure (27.10.2026)

- Drei 3D-Gitter 8x7x6, 12x11x10, 16x15x14; je zwei
  Topologien mit fester Innenhinderniszelle: offene Wasser/Luft-Grenze
  (Ghost-Fluid-Dirichlet, Halbzellenfaktor 2) und geschlossene
  Festkoerper-Neumann-Raender (Druck bis auf additive Konstante).
- Analytisch vorgegebenes raeumliches 3D-Druckfeld erzeugt bekannte
  diskrete MAC-Gradienten; die Projektion muss diese eliminieren.
  Die unabhaengige Python-Referenz berechnet die Ausgangsdivergenz
  separat aus dem Druckfeld, statt sie aus dem C++-Solver abzulesen.
- Neuer MSVC-Release-Nativetest: 38.296 Assertions PASS.
  Nach Projektion max. Divergenz unter 9.3e-11 1/s, kein Festkoerperfluss;
  alle 40/40 CTest-Suites PASS.
- Dokumentation: `docs/P2_MANUFACTURED_PRESSURE_PROJECTION_PARTIAL.md`.
  Gate: `scripts/p2_pressure_projection_acceptance.py`.
  Report: `ENGINE TEST STUBE/runs/p2_manufactured_projection_partial.json`
  (Source-/Binary-/Log-SHA256, `phase_complete=false`).
- **P2 bleibt PARTIAL**: numerischer diskreter Manufactured-Solution-Test
  ist keine externe 3D-CFD-Messreferenz; bewegte Festkoerper-FSI,
  freie-Oberflaechen-Spannungsbedingungen, globale Energie-/Impulsbilanz
  und neue Unreal-5.8-Native-E2E-Abnahme fehlen. P3-P8 ebenso offen.

## P2 Teilfortschritt: statische 3D-MAC-Drucklast an Solid-Voxel (27.10.2026)

- Neue **nur lesende** Drucklast-Diagnose in
  `include/upc/static_pressure_load3d.hpp`: `F = sum(p_fluid * n * dx^2)`
  auf sechs benetzten, achsenparallelen Flaechen eines **statischen**
  Solid-Voxels. Weder Fluid- noch Solid-Bewegung wird veraendert.
- Unabhaengige Manufactured-Pressure-Referenz mit
  `(7,-11,5) Pa/Zelle` und `dx=0.08 m`:
  `(-0.0896,+0.1408,-0.0640) N`. Drei 3D-Raster,
  offene Luft-/geschlossene Neumann-Grenzen, je zwei
  Druck-Offsets; sechs benetzte Flaechen und Gauge-Invarianz
  fuer ein vollstaendig benetztes Hindernis.
- Native-Test: `tests/p2_static_pressure_load3d_tests.cpp`;
  Gate: `scripts/p2_static_pressure_load_acceptance.py`;
  Nachweis: `ENGINE TEST STUBE/runs/p2_static_pressure_load_partial.json`;
  Dokumentation: `docs/P2_STATIC_SOLID_PRESSURE_TRACTION_PARTIAL.md`.
- **P2 bleibt PARTIAL**, P3–P8 bleiben offen. Diese Kraft ist
  Zellmittel-Druck auf statischen Voxel-Flaechen, keine bewegte
  FSI-Reaktion, keine stresskonsistente Oberflaeche, keine globale
  Impuls-/Energieerhaltung, keine externe CFD-Messung oder neue
  Unreal-5.8-Native-E2E-Abnahme.


## P2 Teilfortschritt: statische Mehrvoxel-Druckkraft und Drehmoment (27.10.2026)

- `StaticSolidPressureProbe3D::load_on_body` summiert Druckkraft und
  Drehmoment um einen frei definierten Ursprung auf den benetzten
  Grenzflaechen einer explizit ausgewaehlten statischen Solid-Voxelgruppe.
- Neue 2x2x2-Koerperreferenz mit 24 benetzten Flaechen, drei 3D-Rastern,
  zwei Druck-Offsets und offenen/geschlossenen MAC-Druckgrenzen.
  Analytische Kraft: (-0.5376,+0.8448,-0.3840) N bei dx=0.08 m.
  MSVC Release 42/42 CTest PASS, 11.768 native Assertions,
  188/188 unabhaengige Python-Checks; P1 33/33 erneut PASS.
- Native-Test: `tests/p2_static_body_wrench3d_tests.cpp`;
  unabhaengige Teilabnahme:
  `scripts/p2_static_body_wrench_acceptance.py`;
  Nachweis: `ENGINE TEST STUBE/runs/p2_static_body_wrench_partial.json`.
  Dokumentation: `docs/P2_STATIC_BODY_WRENCH_PARTIAL.md`.
- **P2 bleibt PARTIAL; P3-P8 ebenso.** Die statische Drucklast und
  das statische Drehmoment werden nicht auf einen bewegten Koerper
  angewendet. Kein bidirektionaler FSI-Impuls, keine physikalische
  globale Energievalidierung, keine externen CFD-Messdaten und keine
  neue Unreal-5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: gefrorene Druckkraft-Impulsantwort (27.10.2026)

- Neue isolierte `RigidPressureResponse3D::step` berechnet aus der
  nach echter 3D-MAC-Projektion ermittelten statischen Druckkraft und dem
  Druckdrehmoment eine konstant belastete Starrkoerper-Impulsantwort:
  Translation, Winkelgeschwindigkeit, Druckarbeit, kinetische Energie.
- Ein gleicher und entgegengesetzter Fluidimpuls wird nur als
  **nicht angewandtes Diagnosekonto** ausgegeben. Weder Fluid-MAC noch
  Solid-Voxelmaske oder Koerperorientierung werden aktualisiert.
- Native 12 Faelle: drei 3D-Gitter, Wasser-Luft/Neumann und zwei
  Druck-Offsets; analytische Kraft (-0.5376,+0.8448,-0.3840) N,
  exakte konstant belastete Verschiebung und Arbeitsbilanz.
- MSVC Release Build, 43/43 CTest, 603 native Assertions und
  unabhaengiges Python-Gate 272/272 Checks PASS.
  Report: `ENGINE TEST STUBE/runs/p2_rigid_pressure_response_partial.json`.
  Dokumentation: `docs/P2_FROZEN_PRESSURE_RIGID_RESPONSE_PARTIAL.md`.
- **P2 bleibt PARTIAL**, P3-P8 bleiben unvollstaendig.
  Es fehlen echte bewegte Fluid/Solid-Grenzen, angewandte
  Gegenimpulse, gekoppelte Erhaltung, externe 3D-CFD-Referenzen,
  Rotationsdynamik und UE5.8 Native-E2E dieses neuen Teilpfades.

## P2 Teilfortschritt: tatsächlich angewandter Fluid-Gegenimpuls (27.10.2026)

- `FrozenPressureFluidReaction3D::apply` verteilt den aus
  `RigidPressureResponse3D` stammenden Gegenimpuls **tatsächlich** auf
  gültige wet-wet MAC-Face-Geschwindigkeiten um einen eingefrorenen
  2x2x2-Voxelkörper. Solid-Normal-, Außen- und Luft-Faces bleiben
  unverändert. Alle Eingaben und Overflow-Fälle werden vor Änderungen
  validiert; fehlende zulässige Faces brechen ab.
- Reale projizierte 3D-Druckfelder: 12 Szenarien (3 Raster,
  Wasser-Luft/geschlossene Neumann-Grenzen, 2 Druckoffsets).
  Analytische Kraft (-0.5376,+0.8448,-0.3840) N,
  Körperimpuls bei 0.05 s (-0.02688,+0.04224,-0.01920) Ns,
  wirklich gemessener Fluid-Face-Gegenimpuls entgegengesetzt.
- MSVC Build + 44/44 CTest, 65.400 native Assertions,
  unabhängiges Python-Gate 260/260 Checks PASS;
  P1-Referenz-Gate nach CMake-Änderung erneut 33/33 PASS.
  Bericht: `ENGINE TEST STUBE/runs/p2_applied_fluid_reaction_partial.json`;
  Dokumentation: `docs/P2_APPLIED_FLUID_REACTION_PARTIAL.md`.
- **P2 weiterhin PARTIAL, P3-P8 ebenso.** Die neue Face-Impulsbilanz
  gilt nur **vor** der erneuten Druckprojektion und ist kein
  konservativer Finite-Volume-FSI-Fluss. Keine bewegte Körpermaske,
  keine angewandte Drehimpulsreaktion, keine physikalische globale
  Energie-/Impulsabnahme, keine unabhängigen CFD-Messdaten und
  kein UE5.8-Native-E2E für diesen neuen Pfad.

## P2 Teilfortschritt: angewandte sechsachsige Druck-Wrench-Reaktion (27.10.2026)

- Opt-in `FrozenPressureFluidWrench3D::apply`: lineare **und** angulare
  Gegenimpulse auf die tatsaechlich geaenderten benetzten MAC-Faces,
  diskrete 6x6-Minimalimpuls-Verteilung, kein Eingriff in Solid-Normal-
  oder Wasser-Luft-Faces. Fehlerfaelle bleiben atomar.
- Analytischer 2x2x2-Druckkoerper mit versetztem Drehpunkt, 3 3D-Gitter,
  zwei Randtypen, zwei Druck-Offsets; native und unabhaengige Python-
  Pruefung der sechs Impulsbilanzen, Druck-Gauge, Divergenz und Energie.
- Nachweis: `docs/P2_PRESSURE_WRENCH_REACTION_PARTIAL.md`,
  `scripts/p2_pressure_wrench_reaction_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_pressure_wrench_reaction_partial.json`.
- **P2 bleibt PARTIAL**: nur diskrete Face-Impulsbilanz **vor**
  Reprojektion; feste Koerpermaske, keine vollstaendige dynamische
  FSI-Energie-/Impulserhaltung, keine externen 3D-CFD-Referenzen,
  keine erneute UE5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: Messung der Wrench-Residuen nach MAC-Reprojektion (27.10.2026)

- Neue read-only-Diagnose FrozenMacPostProjectionAudit3D erfasst
  echte Geschwindigkeitsaenderungen an allen internen wet-wet- und
  wet-air-Faces fuer Gegenimpuls, zweite Druckprojektion und Gesamtschritt.
  Lineare/angularen Impulsproxies, Face-Energie und Divergenz werden
  getrennt ausgewiesen. Keine Solver-Aenderung, keine Maske bewegt.
- 12 echte 3D-MAC-Szenarien (3 Gitter, 2 Grenztypen, 2 Druck-Offsets)
  mit analytischem Druckgradienten und sechsachsiger Koerperreaktion.
  Unabhaengige Face-Summation und Teleskopgleichungen geprueft.
- **Wichtiger negativer Befund:** Bei n=8 mit offener Wasser-Luft-Grenze
  bleibt nach Reprojektion ein Koerper-plus-Face-Linearresiduum von
  (-0.026880,+0.0234773,-0.0129658) Ns. Bei geschlossener Geometrie
  hebt die Projektion den angewandten linearen Fluid-Gegenimpuls fast
  vollstaendig wieder auf: Koerper-plus-Face-Linearresiduum bleibt
  (-0.026880,+0.0422400,-0.0192000) Ns; auch das Drehmomentresiduum
  bleibt (-0.0005984,+0.00004608,+0.0013863) Nms.
  Divergenz nach Projektion <9.31e-11 1/s.
- Dokumentation: docs/P2_POST_PROJECTION_WRENCH_AUDIT_PARTIAL.md;
  Gate: scripts/p2_post_projection_audit_acceptance.py;
  Report: ENGINE TEST STUBE/runs/p2_post_projection_audit_partial.json.
- **P2 weiterhin PARTIAL; P3-P8 offen.** Reprojektion ist keine
  nachgewiesene konservative bewegliche FSI-Kopplung. Externe 3D-
  Messreferenzen, physikalische Gesamtenergie, stresskonsistente
  freie Oberflaechen und UE5.8-Native-E2E bleiben offen.

## P2 Teilfortschritt: kinematische bewegte 3D-Solid-Wand (27.10.2026)

- Neuer separater CPU-Operator `KinematicWallMacProjection3D`:
  vorgeschriebene starre Normalgeschwindigkeit `v + omega x r` an
  24 benetzten Grenz-Faces eines 2x2x2-Voxelkörpers, echte 3D-MAC-
  Poisson-Projektion mit Neumann-Wand- und Dirichlet-Luft-Grenze.
  Unveraenderte aeussere Normalwände; Net-Wandvolumenquelle null.
- 12 Szenarien auf 8x7x6 / 12x11x10 / 16x15x14, geschlossen/offen,
  reine Translation/kombinierte Rotation, je auch mit invertierter
  Bewegung: native 95.830 Assertions PASS, maximale Divergenz
  1.164e-12 1/s, 47/47 komplette CTest-Suite PASS,
  unabhaengiges Python-Gate 74/74 Checks PASS.
- Belege: `docs/P2_KINEMATIC_MOVING_WALL_PROJECTION_PARTIAL.md`,
  `scripts/p2_kinematic_wall_projection_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_kinematic_wall_partial.json`.
  P1 wurde nach CMake-Aenderung mit 33/33 Checks erneut geprueft.
- **P2 bleibt PARTIAL; P3-P8 offen.** Nur vorgeschriebene bewegte
  Wandgeschwindigkeit bei **eingefrorener Koerpermaske**. Keine
  swept-volume-Maskenbewegung, keine dynamische Kraft-Rueckwirkung,
  keine globale Impuls-/Energiebilanz, keine externen CFD-Messdaten,
  keine stresskonsistente freie Oberflaeche oder neue UE5.8 Native-E2E.

## P2 Teilfortschritt: lokale Neumann-Kompatibilität getrennter Flüssigkeitskammern (27.10.2026)

- `KinematicWallMacProjection3D` prüft bei eingefrorener Festkörpermaske jetzt **jede zusammenhängende benetzte Komponente** separat auf unvereinbaren Netto-Wandfluss, nicht nur die global aufsummierte Quelle.
- Native Regression: zwei geschlossene, voneinander isolierte Kammern mit gleich großem entgegengesetztem Wandfluss werden vor Mutation abgewiesen; auch eine getrennte offene Kammer verdeckt die inkompatible geschlossene Kammer nicht. Zwei offene Kammern bestehen die analytische Einzelzellen-Druckreferenz ±192 Pa bei ±0,12 m/s. Statische geschlossene Kammern bleiben zulässig.
- Vollständiger MSVC-Release-CTest-Durchlauf: **48/48 PASS**. Eigene Abnahme: `scripts/p2_kinematic_component_acceptance.py`, Nachweis `ENGINE TEST STUBE/runs/p2_kinematic_component_partial.json`; Details `docs/P2_KINEMATIC_COMPONENT_COMPATIBILITY_PARTIAL.md`.
- **P2 weiterhin PARTIAL**, P3–P8 offen: nur kinematische Wandgeschwindigkeiten auf fester Maske, keine tatsächliche Körperverlagerung oder bidirektionale Fluid-/Festkörper-Kopplung, keine vollständige Energie-/Impulsbilanz oder neue Unreal-5.8-Native-E2E-Abnahme.


## P2 Teilfortschritt: Drucktraktion und Wandarbeit der kinematischen MAC-Wand (27.10.2026)

- Neuer read-only-Operator KinematicWallPressureProbe3D verwendet das
  reale Druckfeld von KinematicWallMacProjection3D, summiert
  benetzte Druck-Faces zu Kraft, Drehmoment, Momentanleistung und
  eingefrorenen Impulsen/Arbeit. Keine MAC- oder Solid-Mutation.
- Unabhängige offene Einzellen-Referenz: p = +/-192 Pa,
  F_x = -2.4576 N, P = -0.294912 W, W = -0.0073728 J,
  J_x = -0.06144 Ns bei v_x = +0.12 m/s; Umkehrsymmetrie,
  statische Nullwerte, Gauge-Invarianz eines 24-Face-Körpers
  und P=F.v+Tau.omega geprüft.
- MSVC Release 49/49 CTest PASS, 69 native Assertions.
  Dokumentation: docs/P2_KINEMATIC_WALL_PRESSURE_WORK_PARTIAL.md;
  Gate: scripts/p2_kinematic_pressure_work_acceptance.py;
  Bericht: ENGINE TEST STUBE/runs/p2_kinematic_pressure_work_partial.json.
- P2 bleibt PARTIAL; P3-P8 ebenso. Nur eingefrorene Geometrie,
  keine echte bewegliche Zweiwege-FSI, keine globale Energie-/Impuls-
  validierung, keine externen 3D-Messdaten oder neue UE5.8-Native-E2E.

## P2 Teilfortschritt: atomare Konvergenz der kinematischen MAC-Projektion (27.10.2026)

- Fehlerbehebung: Nach Erschoepfung des Druck-Iterationsbudgets darf ein
  unterkonvergiertes 3D-MAC-Geschwindigkeitsfeld nicht mehr in den
  eigentlichen MAC-Zustand geschrieben werden. Neue Divergenzschranke
  1e-8 s^-1 wird vor dem Face-Writeback geprueft; Fehler lassen
  alle Face-Geschwindigkeiten bitgenau unveraendert.
- Native Regression: 8^3/12^3/16^3, offene und geschlossene
  Fluessigkeitsgeometrie, Translation plus Rotation; 12 bewusst
  zu kurze Druckloesungen werden abgelehnt und alle 81.600
  MAC-Face-Vergleiche bleiben unveraendert. Sechs volle
  Konvergenzlaeufe und stationaerer Ein-Iterations-Nullfall.
- MSVC Release **50/50 CTest PASS**; unabhaengiges Python-Gate,
  Source-SHA256 und Binaerhash: siehe
  `docs/P2_KINEMATIC_PROJECTION_CONVERGENCE_PARTIAL.md`,
  `scripts/p2_kinematic_convergence_acceptance.py` und
  `ENGINE TEST STUBE/runs/p2_kinematic_convergence_partial.json`.
- **P2 bleibt PARTIAL; P3-P8 ebenfalls.** Keine bewegte Maske,
  keine dynamische Zweiwege-FSI, keine externe 3D-CFD-Messvalidierung,
  keine globale Energie-/Impulsbilanz und kein neues UE5.8 Native-E2E.

## P2 Teilfortschritt: kinematischer Druck zu traegem Starrkoerper-Praediktor (27.10.2026)

- Neuer CPU-Adapter LaggedKinematicPressureBody3D: reale konvergierte
  3D-MAC-Projektion der alten starren Wandgeschwindigkeit, Drucktraktion
  und Drehmoment -> aktualisierter Starrkoerper-Zustand. Die MAC-Berechnung
  ist bis zum erfolgreichen Abschluss aller Schritte atomar gekapselt.
- Drei Gitter (8^3/12^3/16^3), beide Bewegungsrichtungen und Nullfall;
  analytische Druck-, Kraft-, Drehmoment-, Impuls- und Geschwindigkeitswerte.
  Der explizite Energie-Lag zwischen alter Wandarbeit und mittlerer
  Starrkoerperarbeit ist +0.0004869586944 J bei +0.12 m/s und wird
  nicht als Fluidenergieerhaltung ausgegeben.
- MSVC Release, 51/51 CTest PASS, 190 native Assertions;
  unabhängiges Gate scripts/p2_lagged_pressure_body_acceptance.py,
  Bericht ENGINE TEST STUBE/runs/p2_lagged_pressure_body_partial.json,
  Details docs/P2_LAGGED_KINEMATIC_PRESSURE_BODY_PARTIAL.md.
- **P2 bleibt PARTIAL, P3-P8 ebenfalls.** Die Koerpermaske bewegt sich
  nicht, der Fluid-Gegenimpuls wird nicht auf MAC-Faces angewandt.
  Keine bidirektionale dynamische FSI, keine globale Impuls-/Energie-
  Erhaltung, keine externen 3D-CFD-Messdaten oder neue UE5.8 Native-E2E.
## P2 Teilfortschritt: konservative Ein-Zellen-Voxelmasken-Verschiebung (27.10.2026)

- Neue eigenstaendige 3D-Rigid-Occupancy-Remap-Komponente
  include/upc/p2_voxel_sweep_remap3d.hpp: exakt ein Voxel entlang einer
  Koordinatenachse, Validierung von Kollisionen, Grenzen und VOF-Masken.
  Einfrierbare/atomare Eingaben; Wasseranteile werden vom neuen
  Fuehrungs-Voxel auf das freigegebene Nachlauf-Voxel derselben
  alten zusammenhaengenden Koerperzeile konservativ umklassifiziert.
- MSVC-Release Build PASS; vollstaendiger 52/52 CTest PASS;
  76.222 native Assertions in 18 3D-Achsen-/Richtungsfaellen.
  Unabhaengige Python-Referenz und SHA256-Nachweise:
  scripts/p2_voxel_sweep_acceptance.py,
  ENGINE TEST STUBE/runs/p2_voxel_sweep_partial.json.
  Detailgrenzen: docs/P2_VOXEL_SWEEP_MASS_REMAP_PARTIAL.md.
- **P2 weiterhin PARTIAL, P3-P8 ebenfalls.** Das Wasser-Remap
  verschiebt die skalare Fuellung kuenstlich durch den Koerper.
  Es ist kein physikalischer lokaler VOF-Flux, keine MAC-Face-Advektion,
  keine dynamische Zweiwege-FSI, keine Impuls-/Energieerhaltung und
  keine neue UE5.8 Native-E2E-Abnahme. Erst die naechsten Arbeiten
  duerfen dieses reine Geometrie-/Massen-Buchhaltungsmodul
  in einen dynamischen Solver integrieren.

## P2 Teilfortschritt: exterior-only 3D-VOF-Routen um bewegte Voxel (27.10.2026)

- Neuer optionaler CPU-Prototyp `ExteriorVoxelSweep3DRemap` vermeidet
  den vorherigen direkten Wassertransport durch den Starrkoerper.
  Stattdessen fuehrt eine sechsfach benachbarte kuerzeste Route ueber
  Zellen ausserhalb der alten UND neuen Koerpermaske und fester Hindernisse.
  Wasseranteile werden entlang des Weges konservativ umklassifiziert;
  blockierte nasse Wege werden atomar abgewiesen, trockene Bewegung bleibt
  ohne Route moeglich. Bestehender Remap unveraendert.
- MSVC Release und **53/53 vollstaendige CTest-Suites PASS**;
  114.764 native Assertions in 18 3D-Achsen-/Richtungsfaellen sowie
  blockierter Nass- und eingeschlossener Trockenfall.
- Belege: `docs/P2_EXTERIOR_VOF_ROUTE_PARTIAL.md`,
  `scripts/p2_exterior_vof_route_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_exterior_vof_route_partial.json`.
- **P2-P8 bleiben PARTIAL.** Auch die exterior-only-Permutation ist
  noch kein zeitaufgeloester physikalischer VOF-Face-Flux, kein MAC-Impuls,
  keine Energieerhaltung oder bidirektionale FSI. Keine neuen externen
  CFD-Messdaten und kein neues Unreal-5.8 Native-E2E.

## P2 Teilfortschritt: konservativer MAC-Face-VOF-Fluss (27.10.2026)

- Neuer isolierter C++-Finite-Volume-Operator `ConservativeVofFlux3D` transportiert VOF mit lokalem Donor-Cell-Fluss ueber echte 3D-MAC-Faces statt durch die Solid-Maske. Feste und aeussere Waende sind undurchlaessig; diskrete Divergenz und ausgehendes CFL werden validiert.
- Analytische Vier-Zellen-Zirkulation in XY/XZ/YZ bei 8^3/12^3/16^3 und beiden Umlaufrichtungen, 3D-Curl-Referenz, Hindernisse und Negativfaelle. Native Assertions: 44653. Unabhaengige Python-Gates: 111/111. MSVC Release und vollstaendige 54/54 CTest PASS. P1 erneut 33/33 PASS.
- Nachweis: `docs/P2_CONSERVATIVE_MAC_VOF_FLUX_PARTIAL.md`, `scripts/p2_conservative_vof_flux_acceptance.py`, `ENGINE TEST STUBE/runs/p2_conservative_vof_flux_partial.json` und vollstaendiges CTest-Protokoll mit SHA256.
- **P2 weiterhin PARTIAL.** Der neue Operator ist optional und arbeitet auf eingefrorener Solid-Maske. Keine bewegte Geometrie, kein Fluid-Gegenimpuls, keine globale Energie-/Impulsbilanz, keine externen 3D-CFD-Messdaten und keine neue UE5.8-Native-E2E-Abnahme. P3-P8 weiterhin PARTIAL.


## P2 Teilfortschritt: lokaler beweglicher 3D-VOF-Grenzfluss mit GCL (28.10.2026)

- Neuer isolierter `MovingBoundaryVofFlux3D` prueft alte/neue binaere
  Starrkoerpermaske und **extern vorgegebene** MAC-Face-Volumenfluesse.
  Jede Zelle muss die diskrete geometrische Volumenbilanz (GCL)
  erfuellen; Wasser wird konservativ mit Donor-Cell-VOF transportiert.
  Kein Wassertransport durch alte Festkoerper-Donoren, neue Solid-Empfaenger,
  feste Hindernisse oder Domain-Grenzen.
- MSVC-Release-Build, **55/55 CTest PASS**, 115318 native Assertions,
  333/333 unabhaengige Python-Referenzchecks fuer 8^3/12^3/16^3,
  alle Achsen und Vorzeichen, 1-/2-Voxel-Koerper, statische Zirkulation,
  CFL/GCL/Fehlerfaelle. P1 erneut mit 33/33 Checks bestaetigt.
- Nachweise: `docs/P2_MOVING_BOUNDARY_GCL_VOF_PARTIAL.md`,
  `scripts/p2_moving_boundary_vof_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_moving_boundary_vof_partial.json`.
- **P2 bleibt PARTIAL**: Face-Fluesse werden noch nicht aus realer
  Koerperkinematik/Fluiddruck berechnet; keine gekoppelte MAC-Impulsbilanz,
  keine globale Energieerhaltung, keine externen 3D-CFD-Messungen und
  kein neuer UE5.8-Native-E2E-Lauf fuer diesen Operator. P3-P8 offen.

## P2 Teilfortschritt: subzellgenaue 3D-MAC-GCL/VOF-Grenzvolumina (28.10.2026)

- Neuer optionaler `FractionalBoundaryVofFlux3D` transportiert Wasser bei
  zeitlich veraenderten offenen Zellvolumenanteilen `alpha_old/alpha_new`
  in [0,1]. Im Unterschied zum binaeren Maskenoperator wird die
  Donor-Saettigung `water_old/alpha_old` korrekt beruecksichtigt.
  Lokale geometrische Erhaltung, Wasser- und offenes Gesamtvolumen,
  Donor-CFL, unzulaessige Faces und Positivitaet werden geprueft.
- MSVC-Release-Build und **56/56 CTest-Suites PASS**; 228858 native
  Assertions sowie **462/462 unabhaengige numerische Referenzchecks**
  auf 8^3/12^3/16^3, alle Achsen und Vorzeichen, Sweeps 0.25/0.5,
  neu geoeffnete Zellen, gesplittete Fluesse, statische Zirkulation
  und Fehlerfaelle. P1 erneut 33/33 PASS.
- Nachweise: `docs/P2_FRACTIONAL_BOUNDARY_GCL_VOF_PARTIAL.md`,
  `scripts/p2_fractional_boundary_vof_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_fractional_boundary_vof_partial.json`
  sowie vollstaendiges CTest-Log mit SHA256.
- **P2 bleibt PARTIAL; P3-P8 offen.** Die offenen Volumenanteile und
  MAC-Face-Fluesse sind weiterhin externe Eingaben, nicht aus einer
  bewegten 3D-Koerperoberflaeche und einem Druck-/Impuls-Solver abgeleitet.
  Keine globale Energie-/Impulsbilanz, externe 3D-CFD-Messvalidierung
  oder neue UE5.8-Native-E2E-Abnahme fuer diesen Operator.


## P2 Teilfortschritt: exakte bewegte Quader-/Voxel-Schnittvolumina (28.10.2026)

- Neue optionale C++-Geometrie `P2MovingBoxOpenVolumes3D` berechnet
  `alpha_old` und `alpha_new` aus zwei tatsaechlichen starren,
  achsparallelen 3D-Quaderpositionen. Sie prueft die volumetrische
  Subzellenueberlappung, konstantes Festkoerpervolumen, Domain-Grenzen,
  starre Translation und Geometrie-CFL.
- MSVC Release und **57/57 CTest PASS**, neuer nativer Test mit
  **76.419 Assertions**, unabhaengiges Python-Gate **120/120**
  numerische Referenzpruefungen (8^3/12^3/16^3, drei Achsen,
  beide Richtungen, 0.25/0.5 Zellverschiebungen, echte
  nicht-gitterausgerichtete Schnittvolumina und Negativfaelle).
- Nachweis: `docs/P2_MOVING_BOX_CUT_CELL_GEOMETRY_PARTIAL.md`,
  `scripts/p2_moving_box_geometry_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_moving_box_geometry_partial.json`,
  `ENGINE TEST STUBE/runs/p2_box_geometry_full_ctest_20261028.txt`.
- **P2 bleibt PARTIAL, P3-P8 ebenfalls.** Das ist ausschliesslich
  Geometrie: Es werden keine MAC-Face-Fluesse, keine Druckprojektion,
  kein Fluid-Gegenimpuls und keine Gesamtenergie erzeugt. Keine
  externen 3D-CFD-Messreferenzen oder neue UE5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: zeitgemittelte MAC-Face-Aperturen bewegter 3D-Quader (28.10.2026)

- Neue optionale `P2MovingBoxFaceApertures3D` berechnet die geometrisch
  **zeitgemittelten** offenen Flaechenanteile aller U/V/W-MAC-Faces
  fuer einen linear translatierten starren, achsparallelen Quader.
  Exakte Intervallzerlegung bei Kantenuebergaengen und
  Gauss-Legendre-Integration der stueckweise quadratischen
  Face-Blockierung. Die alten/neuen subzellgenauen
  offenen Zellvolumina werden aus `P2MovingBoxOpenVolumes3D` uebernommen.
- MSVC-Release-Build und **58/58 CTest PASS**; **1.285.920 native
  Assertions** (inkl. aller Aperturgrenzen und Zeitumkehr);
  **108/108 unabhaengige analytische Face-Integralpruefungen** auf
  8^3/12^3/16^3, allen Normalachsen und Bewegungsrichtungen.
  Referenzen: 1/4, 3/4, 1/2, 35/48, 37/64, 7/16 sowie
  geschlossene, gitterausgerichtete Festkoerper-Faces.
- Nachweis: `docs/P2_MOVING_BOX_MAC_APERTURES_PARTIAL.md`,
  `scripts/p2_moving_box_mac_apertures_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_moving_box_mac_apertures_partial.json`,
  `ENGINE TEST STUBE/runs/p2_box_apertures_full_ctest_20261028.txt`.
- **P2 bleibt PARTIAL; P3-P8 offen.** Die Aperturen sind
  geometrische Flaechenanteile, **keine** physikalischen MAC-Fluesse.
  Bewegte-Wand-GCL, Druckprojektion, Fluid-/Koerper-Gegenimpuls,
  globale Energieerhaltung, externe 3D-CFD-Messdaten und
  neues UE5.8-Native-E2E bleiben offen. Naechster P2-Schritt:
  Aperturen mit bewegter Wandgeschwindigkeit in einen
  konservativen Cut-Cell-Kontinuitaets-/Druckoperator integrieren.

## P2 Teilfortschritt: geometrischer bewegter 3D-Wand-Sweep und lokale GCL (28.10.2026)

- Neuer optionaler `P2MovingBoxSweptWallFlux3DBuilder` erzeugt aus
  exakten zeitgemittelten MAC-Face-Blockierungen und konstanter
  Starrkoerper-Translation die **signierten geometrischen
  Festkoerper-Sweep-Volumina** pro Face. Er verifiziert je Zelle
  `(alpha_new-alpha_old)*dx^3 = div(Q_solid)` und die
  geschlossene Gesamt-/Aussenrand-Volumenbilanz.
- MSVC Release, **59/59 CTest PASS**, **1.816.807 native Assertions**,
  **72/72 unabhaengige analytische Python-Pruefungen** auf 8^3,
  12^3 und 16^3, allen Achsen und beiden Richtungen. Referenzen:
  signierte 3/4- und 1/4-m^3-Sweeps sowie diagonales 45/128-m^3-
  Face-Integral, Zeitumkehr und stationaere/ungueltige Geometrie.
- Nachweis: `docs/P2_MOVING_BOX_SWEPT_WALL_GCL_PARTIAL.md`,
  `scripts/p2_moving_box_swept_wall_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_moving_box_swept_wall_partial.json`,
  `ENGINE TEST STUBE/runs/p2_swept_wall_full_ctest_20261028.txt`.
- **P2 bleibt PARTIAL; P3-P8 bleiben offen.** Dies sind reine
  geometrische Festkoerper-Sweep-Volumina, noch keine
  physikalischen Fluessigkeitsfluesse. Die Integration in
  fraktionale VOF-/MAC-Kontinuitaet und Cut-Cell-Druckprojektion,
  bidirektionaler Impuls, Gesamtenergie, externe 3D-CFD-Messung
  und neues UE5.8-Native-E2E stehen aus.

## P2 Teilfortschritt: geschlossene 3D-Cut-Cell-Druckprojektion mit VOF (28.10.2026)

- Neuer optionaler `P2MovingBoxCutCellProjection3DBuilder` koppelt exakte
  bewegte Quader-Cut-Cell-Volumina, zeitgemittelte offene MAC-Faces und
  geometrischen Wand-Sweep mit einem **gewichteten 3D-Druck-Poisson**
  (Jacobi-PCG). Er erzeugt echte Fluid-Face-Volumenfluesse, deren lokale
  Divergenz `alpha_old-alpha_new` kompensiert; Aussenraender geschlossen.
- In 36 Bewegungsfaellen (8^3/12^3/16^3, alle Achsen/Vorzeichen,
  0.25/0.5 Zellverschiebungen) wurden die neuen Face-Fluesse auch an
  `FractionalBoundaryVofFlux3D` uebergeben. Bei vollstaendig gesaettigtem
  Wasser reproduziert der VOF-Operator alle neuen offenen Zellvolumina
  bei konstanter Gesamtwassermenge und gepruefter Donor-CFL.
- **60/60 CTest PASS**, **489.969 native Assertions** und unabhaengiges
  Python-Gate fuer 36 analytische 3D-Volumenreferenzen (144 Checks).
  Dokumentation: `docs/P2_CUT_CELL_PRESSURE_VOF_PARTIAL.md`;
  Gate: `scripts/p2_cut_cell_projection_acceptance.py`;
  Report: `ENGINE TEST STUBE/runs/p2_cut_cell_projection_partial.json`.
- **P2-P8 bleiben PARTIAL.** Geschlossene, vollstaendig nasse
  Anfangsruhe-Situation mit einem linear bewegten achsparallelen Quader;
  kein allgemeiner Live-MAC/VOF-Step, keine freie Oberflaeche,
  keine dynamische Zweiwege-FSI/Impuls-/Energieabnahme,
  keine externen CFD-Messdaten und kein neues UE5.8 Native-E2E.

## P2 Teilfortschritt: MAC-Prädiktor in bewegter Cut-Cell-Druckprojektion (28.10.2026)

- `P2MovingBoxCutCellProjection3DBuilder::sample_with_predictor` projiziert
  jetzt optional extern vorgegebene, **nichtnullige 3D-MAC-Geschwindigkeiten**
  auf die bewegte Cut-Cell-Kontinuitätsgleichung. Zeitintegrierte
  Prädiktorflüsse berücksichtigen die zeitgemittelte offene Face-Fläche.
  Der bisherige Nullprädiktor-Pfad `sample` bleibt kompatibel.
- Unabhängige diskrete 3D-Gradient/Curl-Manufactured-Tests auf
  8³/12³/16³ für 36 Körperbewegungen: Gradientenanteil wird entfernt,
  divergenzfreie Vier-Face-Zirkulation erhalten; Druckdifferenz -480 Pa.
  Vollständige **61/61 CTest**, **396.978 native Assertions**,
  **144/144 unabhängige numerische Checks** bestanden.
- Nachweise: `docs/P2_PREDICTED_MAC_CUT_CELL_PROJECTION_PARTIAL.md`,
  `scripts/p2_predictor_projection_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_predictor_projection_partial.json`.
- **P2 bleibt PARTIAL:** Noch keine Einbindung des Prädiktors in einen
  zeitaufgelösten Live-CoupledMacWater3D-Solver, keine freien Oberflächen,
  keine physikalisch validierte Körper-Gegenreaktion, globale Energie-/
  Impulsbilanz, unabhängigen 3D-CFD-Messdaten oder neue UE5.8-Native-E2E.
  P3–P8 bleiben ebenfalls Vollphasen-offen.

## P2 Teilfortschritt: sequentielle vorgeschriebene Cut-Cell-MAC/VOF-Schritte (28.10.2026)

- Neuer optionaler `P2PrescribedBoxMacVof3D` verbindet die vorhandene
  Druckprojektion fuer eine bewegte achsparallele Box mit dem
  fraktionalen 3D-VOF-Operator ueber mehrere Zeitschritte. Die
  Wasseranteile und subzellgenauen offenen Volumina werden weitergereicht;
  fehlerhafte Schritte hinterlassen keinen geaenderten Zustand.
- Native Tests: 8^3/12^3/16^3, alle Bewegungsachsen und Vorzeichen,
  50/100 Prozent Saettigung, zwei Zeitschritte, synthetischer
  Gradient/Curl-Praediktor, unabhaengige Wasserreferenz
  `saturation*(n^3-1.5^3)` und Rollback-Negativfaelle.
- Dokumentation: `docs/P2_PRESCRIBED_SEQUENCE_PARTIAL.md`;
  Gate: `scripts/p2_prescribed_sequence_acceptance.py`;
  Report: `ENGINE TEST STUBE/runs/p2_prescribed_sequence_partial.json`.
- **P2 bleibt PARTIAL, P3-P8 ebenfalls:** vorgeschriebene Koerperbahn
  und externer Praediktor, keine Einbindung in den allgemeinen
  CoupledMacWater3D-Live-Solver, keine freie Oberflaeche, keine
  bidirektionale FSI, keine globale Energie-/Impulsabnahme,
  keine externen CFD-Messdaten und keine neue UE5.8-Native-E2E.

## P2 Teilfortschritt: konservativer periodischer 3D-MAC-Lineartransport (28.10.2026)

- Neuer isolierter `P2PeriodicMacTransport3D`: konservativer expliziter
  3D-Upwind-Transport aller drei gestaffelten MAC-Komponenten mit konstanter,
  extern vorgeschriebener Transportgeschwindigkeit und periodischen Grenzen.
  Redundante MAC-Grenzflächen werden korrekt gespiegelt; Eingaben bleiben
  bei CFL-/Form-/Seam-Fehlern unverändert.
- Diskrete unabhängige Referenz: dreidimensionale alternierende Eigenmode
  mit `G=0.38` pro Schritt, Mittelwert-/Summenerhaltung und quadratischer
  Dissipation `G^(2k)` auf drei nichtkubischen Gittern, acht
  Transportrichtungen und fünf Schritten.
- Native MSVC Release, CTest und unabhängiges Python-Gate:
  `docs/P2_PERIODIC_MAC_LINEAR_TRANSPORT_PARTIAL.md`,
  `scripts/p2_periodic_mac_transport_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_periodic_mac_transport_partial.json`.
- **P2 bleibt PARTIAL, P3–P8 ebenfalls offen.** Nur lineare periodische
  Advektion, noch keine nichtlineare lokale MAC-Impulsadvektion,
  bewegte Cut-Cell-Wände, freie Oberflächen, Zweiwege-FSI,
  unabhängigen CFD-Messdaten oder neue UE5.8-Native-E2E-Abnahme.


## P2 Teilfortschritt: variable konservative periodische MAC-Impulsadvektion (28.10.2026)

- Neuer optionaler P2VariableMacTransport3D: ortsvariable, diskret divergenzfreie
  3D-MAC-Transportgeschwindigkeit auf versetzten Kontrollvolumina; konservativer
  lokaler Upwind-Fluss, periodische Randbedingungen und lokale ausgehende CFL.
- Isolierte nichtlineare self_advect-Einzelstufe, ohne Druckprojektion.
  Analytische Scher-Fourier-Referenz mit vier Schritten und drei nichtkubischen
  Gittern, Selbstadvektions-Impulssummen, Negativ- und Rollback-Tests.
- MSVC Release, 65/65 CTest; separates Gate:
  scripts/p2_variable_mac_transport_acceptance.py,
  ENGINE TEST STUBE/runs/p2_variable_mac_transport_partial.json.
  Details: docs/P2_VARIABLE_MAC_FLUX_TRANSPORT_PARTIAL.md.
- P2 bleibt PARTIAL; P3-P8 bleiben PARTIAL. Keine Einbindung in
  CoupledMacWater3D, keine bewegten Cut-Cells, freien Oberflaechen,
  Zweiwege-FSI, externen 3D-Messdaten oder neuen UE5.8 Native-E2E.

## P2 Teilfortschritt: periodische nichtlineare 3D-MAC-Advektion und PCG-Druckprojektion (28.10.2026)

- Neuer optionaler P2PeriodicMacProjection3D: matrixfreier periodischer
  3D-Poisson-Operator mit Jacobi-PCG, Druck-Gauge und MAC-Face-Korrektur.
- self_advect_project verbindet den konservativen variablen
  MAC-Impulstransport mit der Divergenzprojektion ueber mehrere Schritte.
- Unabhaengige analytische Fourier-Druckreferenzen, drei nichtkubische
  3D-Gitter, vier nichtlineare Folgeschritte, Massensummen-Proxies,
  Nullraum-/Negativfaelle und vollstaendiger nativer CTest-Regressionslauf.
- Nachweise: docs/P2_PERIODIC_NONLINEAR_MAC_PROJECTION_PARTIAL.md,
  scripts/p2_periodic_projection_acceptance.py,
  ENGINE TEST STUBE/runs/p2_periodic_projection_partial.json.
- P2 bleibt PARTIAL, P3-P8 ebenso. Periodische vollstaendig nasse
  konstantdichte Testdomain; keine freien Oberflaechen, bewegten
  Cut-Cells, bidirektionale FSI, globale physikalische Impuls-/
  Energiebilanz, externe 3D-CFD-Messdaten oder neues UE5.8 Native-E2E.

## P2 Teilfortschritt: periodische nichtlineare 3D-MAC/VOF-Kopplung (28.10.2026)

- Opt-in `P2PeriodicMacVof3D`: nichtlineare MAC-Selbstadvektion, PCG-Druckprojektion und konservativer passiver Wasserfraktions-Transport mit den **projizierten** 3D-MAC-Face-Geschwindigkeiten.
- MSVC Release **67/67 CTest PASS**, **65.804 native Assertions**, drei nichtkubische Gitter, beide konstanten Flussrichtungen, vier Zeitschritte, drei nichtlineare Scherfeldfaelle mit vier Schritten, zellweise unabhaengige Upwind-Referenz und 7 Negativtests.
- Maschinenlesbares Teil-Gate: `scripts/p2_periodic_mac_vof_acceptance.py`, `ENGINE TEST STUBE/runs/p2_periodic_mac_vof_partial.json`, 21 unabhaengige numerische Kontrollen, Source-/CTest-Hashes, `phase_complete=false`. Details: `docs/P2_PERIODIC_PROJECTED_MAC_VOF_PARTIAL.md`.
- **P2 bleibt PARTIAL**: periodischer, konstantdichter passiver VOF-Marker ist keine physikalische freie Wasseroberflaeche; keine bewegten Cut-Cells, keine dynamische FSI, keine unabhaengigen 3D-Messreferenzen und kein neues UE5.8-Native-E2E. P3-P8 bleiben ebenfalls PARTIAL.

## P2 Teilfortschritt: periodische MAC/VOF-Druckarbeitsbilanz (28.10.2026)

- Optionaler P2PeriodicMacVofEnergy3D-Audit: konstante Dichte,
  periodische 3D-MAC-Faces, nichtlineare Upwind-Impulsadvektion,
  PCG-Druckprojektion und passiver konservativer VOF-Transport.
- Diskrete kinetische Energien vor/nach Advektion/Projektion,
  Druckarbeit ueber Zell-Divergenz, positive quadratische
  Druckkorrekturenergie, Orthogonalitaets- und Gradient-Residuen.
  Periodische Seam-Kopien werden nur einmal gezaehlt.
- Native Tests auf 6x7x8, 8x9x10, 10x12x14 mit je vier
  nichtlinearen Schritten und analytisch hergestellten 3D-Druckfeldern;
  Negativtests fuer Druckmanipulation, periodische Naht,
  ungueltige Dichte/Wasserfraktion und vollstaendige Regression.
- Belege: docs/P2_PERIODIC_MAC_VOF_ENERGY_PARTIAL.md,
  scripts/p2_periodic_mac_vof_energy_acceptance.py,
  ENGINE TEST STUBE/runs/p2_periodic_mac_vof_energy_partial.json.
- **P2 bleibt PARTIAL, P3-P8 ebenso.** Keine freie Oberflaeche,
  Cut-Cell-Koerperbewegung, echte FSI-Gegenreaktion, globale
  thermo-mechanische Energiebilanz, externen CFD-Messdaten oder
  neue UE5.8-Native-E2E-Abnahme fuer diesen Pfad.

## P2 Teilfortschritt: bewegte Cut-Cell-Druckarbeit und Energie-Audit (28.10.2026)

- Neuer optionaler P2MovingCutCellPressureEnergy3D-Audit: geschlossene
  3D-Cut-Cell-Domaene, zeitgemittelte MAC-Flaechenoeffnungen, vorgeschriebene
  Starrkoerper-Translation, Druckgradient, lokale Volumenkontinuitaet,
  geometrische Druckarbeit und gewichtete diskrete Face-Energie.
- Identitaet: K_nach-K_vor = W_geometrisch - K_druckkorrektur.
  Nullpraediktor-Referenz: K_nach=K_korrektur, W_geom=2*K_korrektur.
  Unabhaengig hergestellter Gradient/Zirkulation: K_vor=56 J.
- Opt-in P2AuditedPrescribedBoxMacVof3D prueft MAC/VOF + Energiebilanz
  vor dem Commit des naechsten Zustands (transaktionaler Rollback).
- MSVC Release **69/69 CTest PASS**, 76.618 native Assertions,
  8^3/12^3/16^3, drei Achsen, beide Richtungen, zwei Predictor-Modi,
  zwei sequentielle VOF-Schritte, Druck-Gauge- und Negativfaelle.
- Nachweis: docs/P2_MOVING_CUT_CELL_PRESSURE_ENERGY_PARTIAL.md,
  scripts/p2_moving_cut_cell_energy_acceptance.py,
  ENGINE TEST STUBE/runs/p2_moving_cut_cell_energy_partial.json.
- phase_gate.py nutzt nun paralleles CTest (4 Jobs, 180-s-Timeout)
  statt des zuvor nach 90 s abgebrochenen seriellen Gesamtlaufs.
- **P2 weiterhin PARTIAL, P3-P8 PARTIAL**: nur diskrete Druckidentitaet,
  keine vollstaendige physikalische FSI-Energie, keine Cut-Cell-
  Impulsadvektion, freien Oberflaechen, externen CFD-Messdaten oder
  neue Unreal-5.8-Native-E2E fuer diesen Pfad.

## P2 Teilfortschritt: signierter Skalartransport auf bewegten Cut-Cells (28.10.2026)

- Neuer optionaler zellzentrierter 3D-Upwind-Operator auf den
  projizierten, zeitintegrierten Cut-Cell-MAC-Volumenfluessen eines
  linear verschobenen Starrkoerperquaders. Er prueft GCL, Donor-CFL,
  globale signierte Skalarerhaltung und konstante Feldwerte.
- Drei Gitter (8^3/12^3/16^3), drei Achsen, beide Richtungen,
  je zwei Schritte, unabhaengige Upwind- und geometrische
  Quader-Schnittvolumen-Referenzen sowie Negativfaelle.
- Beleg: docs/P2_MOVING_CUT_CELL_SIGNED_SCALAR_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_moving_cut_cell_scalar_partial.json.
- **P2 weiterhin PARTIAL.** Dies ist noch kein MAC-Dualvolumen-
  Impulstransport und keine physikalische FSI-/Energiekopplung.
  P3-P8 ebenfalls PARTIAL; ALL_COMPLETE=False.

## P2 Teilfortschritt: exakte bewegte MAC-Dualvolumina (28.10.2026)

- Optionaler geometrischer P2MovingMacDualVolumes3DBuilder: echte um
  dx/2 versetzte u-/v-/w-Kontrollvolumina mit exakten Quader-
  Schnittvolumina, zeitintegriertem Wand-Sweep und lokalem GCL.
- Vollstaendiger MSVC-Release-CTest und analytische unabhaengige
  3D-Dualgeometrie-Referenzen: docs/P2_MOVING_MAC_DUAL_VOLUME_PARTIAL.md.
- Belege: scripts/p2_moving_mac_dual_acceptance.py und
  ENGINE TEST STUBE/runs/p2_moving_mac_dual_partial.json.
- **P2 bleibt PARTIAL**, ebenso P3-P8. Es fehlen physikalische
  Dual-MAC-Impulsadvektion, Wand-Gegenimpuls, freie Oberflaeche,
  externe CFD-Messvalidierung und neues UE5.8-Native-E2E.

## P2 Teilfortschritt: konservativer Transport auf bewegten MAC-Dualvolumina (28.10.2026)

- `P2MovingMacDualTransport3D`: separat projizierte geschlossene
  Cut-Cell-Volumenfluesse fuer drei gestaffelte Dualgitter und konservativer
  signierter Upwind-Transport. 72/72 CTest, 216 Bewegungsreferenzen.
- Belege: docs/P2_MOVING_MAC_DUAL_TRANSPORT_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_moving_mac_dual_transport_partial.json.
- **P2 bleibt PARTIAL**: separate Null-Praediktoren, keine einheitliche
  physikalische MAC-Selbstadvektion oder FSI-Gegenreaktion.

## P2 Teilfortschritt: gemeinsamer Primal-MAC-Praediktor fuer Dualgitter (28.10.2026)

- `P2MovingMacSharedPredictor3D` interpoliert EIN primal gestaffeltes
  u/v/w-Feld auf die drei bewegten Dualgitter; separate Cut-Cell-
  Druckprojektionen und konservativer signierter Transport bleiben erhalten.
- MSVC-Release und 73/73 CTest PASS; 268716 native Assertions,
  108 konservative Bewegungs-/Schritt-/Komponentenfaelle und analytische
  Halbzellen-Interpolationspruefung. P1 erneut 33/33 PASS.
- Belege: docs/P2_MOVING_MAC_SHARED_PREDICTOR_PARTIAL.md,
  scripts/p2_moving_mac_shared_predictor_acceptance.py und
  ENGINE TEST STUBE/runs/p2_moving_mac_shared_predictor_partial.json.
- **P2 weiterhin PARTIAL, P3-P8 ebenfalls PARTIAL.** Drei unabhaengig
  druckkorrigierte Dualfluesse sind noch kein einheitlicher physikalischer
  Impulstensor. Naechster Schritt: konsistente gemeinsame Impulsfluesse,
  dynamische Wand-Gegenreaktion, freie Oberflaeche, CFD-Messdaten und
  Native-E2E fuer den voll gekoppelten Pfad.

## P2 Teilfortschritt: Primal-MAC-Druckprojektion vor gemeinsamer Dual-Rekonstruktion (28.10.2026)

- `P2MovingMacSharedPredictor3D::project_primal` projiziert optional
  das gemeinsame Primal-u/v/w-Feld zuerst mit dem tatsaechlichen
  bewegten Cut-Cell-GCL und rekonstruiert danach korrigierte
  Primal-Face-Geschwindigkeiten aus zeitintegrierten Volumenfluessen.
- `step_projected_primal` gibt diese EINMAL primal projizierten
  Geschwindigkeiten an die drei Dualgitter weiter; die anschliessenden
  Dual-Projektionen bleiben explizit voneinander unabhaengig.
- MSVC Release 73/73 CTest PASS; 312870 native Assertions im
  erweiterten gemeinsamen Test, 12 bewegte 3D-GCL-Faelle,
  zwei analytische Druckgradient-Nullraumreferenzen und
  33/33 unabhaengige Teil-Gate-Pruefungen PASS.
- Nachweise: docs/P2_PRIMAL_PROJECTED_SHARED_PARTIAL.md,
  scripts/p2_primal_projected_shared_acceptance.py und
  ENGINE TEST STUBE/runs/p2_primal_projected_shared_partial.json.
- **P2 weiterhin PARTIAL; P3-P8 offen; ALL_COMPLETE=False.**
  Keine einheitliche korrigierte Impulsfluss-Tensorbilanz,
  dynamische Fluid-Solid-Gegenreaktion, freie Oberflaeche,
  externe 3D-CFD-Messvalidierung oder neues UE5.8 Native-E2E.

## P2 Teilfortschritt: messbare Primal-/Dual-MAC-Flux-Konsistenzluecke (28.10.2026)

- Neuer optionaler `P2MovingMacFluxConsistency3D`-Audit vergleicht
  einen gemeinsamen primal druckprojizierten MAC-Praediktor mit den
  drei separat projizierten bewegten MAC-Dualvolumenfluessen.
- Nachweis: MSVC Release, **74/74 CTest PASS**, 120467 native Assertions,
  12 bewegte 3D-Faelle (8^3/12^3, 3 Achsen, 2 Richtungen), zwei
  stationaere Nullreferenzen und unabhaengige Flusssummenpruefungen.
  29/29 quantitative Teilabnahme-Checks; Quell-/Log-SHA256.
- Belege: docs/P2_MOVING_MAC_FLUX_CONSISTENCY_PARTIAL.md,
  scripts/p2_moving_mac_flux_consistency_acceptance.py und
  ENGINE TEST STUBE/runs/p2_moving_mac_flux_consistency_partial.json.
- **P2 bleibt PARTIAL, P3-P8 ebenfalls PARTIAL; ALL_COMPLETE=False.**
  Das Messinstrument behebt die gemeinsame Impulstensor-Luecke nicht.
  Freie Oberflaechen, echte Fluid-Solid-Gegenreaktion, globale Energie,
  externe CFD-Referenzen und Unreal-5.8-Native-E2E fehlen weiterhin.

## P2 Teilfortschritt: numerische MAC-Dualenergie-Stabilitaet (28.10.2026)

- Neuer optionaler `P2MovingMacDualEnergy3D`-Audit misst
  volumen-gewichtete signierte Dual-Komponentenimpulse und
  quadratische kinetische L2-Proxies vor/nach bewegtem Upwind-Transport.
- Prueft getrennte Komponentenerhaltung, Donor-CFL, GCL und
  nicht zunehmende diskrete L2-Energie (Jensen/konvexer Upwind).
- MSVC Release, 75/75 CTest, 8^3/12^3, alle Bewegungsachsen und
  Richtungen, uniforme/nichtuniforme signierte Komponenten und
  je zwei sequentielle Schritte. Unabhaengige Nachrechnung und
  Quell-/CTest-Hashes in docs/P2_MOVING_MAC_DUAL_ENERGY_PARTIAL.md.
- **P2 weiterhin PARTIAL, P3-P8 PARTIAL, ALL_COMPLETE=False.**
  Dies ist nur eine numerische Upwind-Stabilitaetsbilanz, keine
  physikalische Druck-/FSI-Energie, keine gemeinsame Impulstensor-
  Kopplung, keine freien Oberflaechen, externe 3D-CFD-Referenz oder
  UE5.8-Native-E2E fuer diesen Pfad.

## P2 Teilfortschritt: diskrete Druckarbeit auf drei bewegten MAC-Dualgittern (28.10.2026)

- Neuer optionaler `P2MovingMacDualPressureWork3D`-Audit: gemeinsame
  Primal-MAC-Druckprojektion, Halbzellen-Rekonstruktion und separate
  Druckkorrekturen auf allen drei bewegten Dualgittern.
- Pro Dualgitter wird die exakte diskrete Druckgradient-/Wandarbeits-
  Identitaet geprueft; aggregierte Projektionsenergie, geometrische
  Druckarbeit und Korrekturenergie werden separat ausgewiesen.
- MSVC Release und vollstaendige 76/76 CTest-Suites PASS; 14 3D-Faelle
  (8^3/12^3, drei Achsen, beide Richtungen, zwei statische Referenzen),
  Druck-Gauge- und Negativtests. Quantitative Nachweise und SHA-256:
  docs/P2_MOVING_MAC_DUAL_PRESSURE_WORK_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_moving_mac_dual_pressure_work_partial.json.
- **P2 bleibt PARTIAL; P3-P8 PARTIAL; ALL_COMPLETE=False.**
  Drei separate Dual-Druckkorrekturen sind noch kein gemeinsamer
  physikalischer Impulstensor. Keine dynamische Fluid-Solid-Gegenreaktion,
  spannungskorrekte freie Oberflaeche, externe 3D-CFD-Messvalidierung
  oder UE5.8-Native-E2E dieses Pfades.

## P2 Teilfortschritt: Drucktraktion und Gegenimpuls-Diagnose (29.10.2026)

- `P2MovingBoxPressureTraction3D` integriert stueckweise konstanten
  Druck auf den sechs Oberflaechen eines vorgeschrieben bewegten
  achsenparallelen Quaders. Exakte Oberflaechen-/Voxel-Schnittflaechen,
  resultierende Kraft, Drehmoment, diagnostischer Fluid-Gegenimpuls
  und exakte diskrete geometrische Druckarbeit werden getrennt erfasst.
- MSVC Release und vollstaendiger 77/77 CTest PASS. 12 unabhaengige
  analytische Bewegungsfaelle auf 8^3/12^3 (3 Achsen, beide Richtungen),
  Kraft 40 N, Gegenimpuls -10 kg m/s, Arbeit +/-8 J; Druck-Gauge,
  asymmetrischer Drehmoment-Patch, echter PCG-Druck und Negativfaelle.
- Belege: docs/P2_MOVING_BOX_PRESSURE_TRACTION_PARTIAL.md,
  scripts/p2_moving_box_pressure_traction_acceptance.py und
  ENGINE TEST STUBE/runs/p2_moving_box_pressure_traction_partial.json.
- **P2 weiterhin PARTIAL; P3-P8 PARTIAL; ALL_COMPLETE=False.**
  Gegenimpuls wird NICHT angewendet, keine dynamische FSI, keine
  vollstaendige physikalische Energie-/Impulsbilanz, freie Oberflaeche,
  externe 3D-CFD-Messvalidierung oder neues UE5.8-Native-E2E.

## P2 Teilfortschritt: Druckgetriebene dynamische Quadertranslation (29.10.2026)

- `P2MovingBoxDynamicPressure3D` integriert die an der alten Cut-Cell-
  Boxoberflaeche gemessene Druckkraft tatsaechlich in die lineare
  Geschwindigkeit und den Schwerpunkt eines starren Quaders.
  Aktualisierte Geometrie und Druckarbeit werden separat geprueft.
- 12 zweischrittige analytische 3D-Faelle: 8^3/12^3, alle Achsen und
  Druckgradient-Vorzeichen; Kraft +/-25.6 N, Endgeschwindigkeit
  +/-0.512 m/s, kinetische Energie 1.31072 J. Gauge-, Ballistik-,
  Drehmoment-Ablehnungs- und Transaktions-Tests.
- MSVC Release und 78/78 CTest PASS. Belege:
  docs/P2_MOVING_BOX_DYNAMIC_PRESSURE_PARTIAL.md,
  scripts/p2_moving_box_dynamic_pressure_acceptance.py,
  ENGINE TEST STUBE/runs/p2_moving_box_dynamic_pressure_partial.json.
- **P2 weiterhin PARTIAL, P3-P8 PARTIAL, ALL_COMPLETE=False.**
  Fluid-Gegenimpuls ist NUR ein Ledger, nicht auf MAC/VOF angewendet.
  Keine vollstaendige bidirektionale FSI, keine globale Energie-/
  Impulserhaltung, keine externe CFD-Messvalidierung oder neues UE-E2E.

## P2 Teilfortschritt: angewendeter MAC-Dual-Gegenimpuls (29.10.2026)

- Optionales `P2MovingBoxMacCounterImpulse3D` verbindet die dynamische
  Druckkraft-/Quadertranslation mit einem tatsaechlichen Update der
  zurueckgegebenen MAC-Face-Geschwindigkeiten. Der Gegenimpuls wird
  auf offene bewegte Dualvolumenmassen nahe der Box verteilt.
- MSVC Release 79/79 CTest PASS; 663 native Assertions und 12
  zweischrittige analytische 3D-Referenzfaelle (8^3/12^3, alle Achsen,
  beide Druckgradient-Vorzeichen). Je zwei Schritte ergeben
  Quaderimpuls +/-5.12 Ns und angewendeten Fluid-Face-Impuls -/+5.12 Ns.
- Der fehlende Projektionsschritt wird durch ein ausdruecklich
  NICHT verschwindendes geometrisches Kontinuitaetsresiduum gezeigt.
  Dies ist nur eine diskrete Impulsproxy-Bilanz, keine echte FSI.
- Belege: docs/P2_MOVING_BOX_MAC_COUNTERIMPULSE_PARTIAL.md,
  scripts/p2_moving_box_mac_counterimpulse_acceptance.py und
  ENGINE TEST STUBE/runs/p2_moving_box_mac_counterimpulse_partial.json.
- **P2 weiterhin PARTIAL; P3-P8 PARTIAL; ALL_COMPLETE=False.**
  Es fehlen ein gemeinsamer konservativer physikalischer Impulstensor,
  die anschliessende Cut-Cell-Projektion, VOF-Massenkopplung, globale
  Energie-/Impulserhaltung, gemessene 3D-CFD-Referenzen und UE5.8-E2E.

## P2 Teilfortschritt: dynamischer MAC-Gegenimpuls mit nachfolgender Cut-Cell-Projektion (29.10.2026)

- `P2MovingBoxProjectedReaction3D`: angewendeter MAC-Gegenimpuls
  wird mit gemeinsamer primal-Cut-Cell-PCG-Druckprojektion auf die
  geometrische Kontinuität zurückgeführt; druckbedingte Änderung
  des Dualmassen-Impulsproxies separat dokumentiert.
- MSVC Release 80/80 CTest PASS; 12 analytische 3D-Fälle auf 8³/12³,
  3 Achsen, beide Druckgradientvorzeichen, unabhängig nachgerechnete
  Voxel-GCL, Gauge-Invarianz und Negativfälle.
- **Explizit nicht geschlossen:** die Druckprojektion ändert den
  Fluid-Impulsproxy stark. Kein globaler physikalischer FSI-Impuls-/
  Energiebeweis; keine Live-VOF-Kopplung, externen 3D-CFD-Messdaten
  oder Unreal-5.8-Native-E2E. P2-P8 PARTIAL, ALL_COMPLETE=False.
- Belege: `docs/P2_MOVING_BOX_PROJECTED_REACTION_PARTIAL.md`,
  `scripts/p2_moving_box_projected_reaction_acceptance.py` und
  `ENGINE TEST STUBE/runs/p2_moving_box_projected_reaction_partial.json`.

## P2 Teilfortschritt: projizierter dynamischer Quader und konservativer passiver VOF-Transport (29.10.2026)

- Opt-in `P2MovingBoxProjectedVof3D` verbindet den angewendeten
  MAC-Gegenimpuls, die gemeinsame bewegte Cut-Cell-Projektion und den
  konservativen skalaren Wasser-Fuellfraktions-Remap auf denselben Fluxes.
- 24 analytische Einzelschrittfaelle (8^3/12^3, 3 Achsen, +/- Druckgradient,
  voll geflutet oder raeumlich variabler Wasseranteil), 12 zweischrittige
  Faelle mit aktualisiertem Koerper/MAC/VOF; unabhaengige zellweise
  Finite-Volumen-Neuberechnung, Wassermasse, GCL, Bounds, CFL und
  Negativtests. Vollstaendige CTest- und quantitative Nachweise:
  docs/P2_MOVING_BOX_PROJECTED_VOF_PARTIAL.md,
  scripts/p2_moving_box_projected_vof_acceptance.py und
  ENGINE TEST STUBE/runs/p2_moving_box_projected_vof_partial.json.
- **P2 bleibt PARTIAL, P3-P8 PARTIAL, ALL_COMPLETE=False.**
  Nur passiver Wassertransport; keine freie Wasser-Luft-Spannungsgrenze,
  keine druckseitige Rueckkopplung der Wasserfraktion, kein geschlossener
  physikalischer Fluid-Festkoerper-Impuls/Energiehaushalt, keine externen
  3D-CFD-Messdaten und kein Unreal-5.8-Native-E2E dieses Pfades.

## P2 Teilfortschritt: stratified hydrostatic VOF-Drucklast auf Starrkoerper (29.10.2026)

- Optionaler P2StratifiedHydrostaticLoad3D-Pfad: Wasser-Fuellfraktionen
  aus vertikal geschichteten, ruhenden Wassersaeulen erzeugen
  hydrostatischen Druck und treiben damit die reale Quadertranslation.
  Die Kraft wird an den echten Quaderflaechen integriert (Archimedes),
  nicht aus zellzentrischen Druckwerten als Traktion geschaetzt.
- Analytische Referenzen 8^3/12^3, trockene/teilgefuellte/vollgefuellte
  Saeulen und g=0/9.81; native Tests, volle CTest-Suite und
  SHA256-Nachweise in docs/P2_STRATIFIED_HYDROSTATIC_PARTIAL.md.
  MSVC Release und 82/82 CTest PASS; 13.669 native Assertions,
  53 quantitative Gate-Checks, 12 analytische Referenzfaelle.
- **P2-P8 bleiben PARTIAL, ALL_COMPLETE=False.** Nur statischer
  hydrostatischer Lastpfad; Gegenimpuls nicht an MAC zurueckgegeben,
  keine freie Oberflaechenstroemung, keine globale FSI-Erhaltung,
  keine externen 3D-Messreferenzen oder UE5.8-Native-E2E.

## P2 Teilfortschritt: wasservolumengemittelter hydrostatischer Teilzellen-Druck (29.10.2026)

- Der optionale `P2StratifiedHydrostaticLoad3D`-Diagnosepfad integriert
  den hydrostatischen Druck in teilweise gefuellten VOF-Oberflaechenzellen
  ueber das **benetzte** Zellvolumen. Bisher wurde in solchen Zellen
  faelschlich am geometrischen Zellzentrum (ggf. oberhalb der
  Wasserlinie) ausgewertet; neu `p=rho*g*f*dx/2` fuer `0<f<1`.
- Unabhaengige Spaltenreferenz `sum_z(p_z*f_z*dx)=rho*g*h^2/2`.
  24 analytische 3D-Konfigurationen, 8^3/12^3, trockene/ganzzahlige/
  fraktionale Wasserlinien und g=0/9.81. MSVC Release und 82/82 CTest
  PASS; 29.821 native Assertions, 101 quantitative Checks. Teilabnahme mit
  Source-SHA256: `scripts/p2_stratified_hydrostatic_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_stratified_hydrostatic_partial.json`.
- **P2 bleibt PARTIAL; P3-P8 PARTIAL; ALL_COMPLETE=False.**
  Keine dynamische 3D-Freiflaechenstroemung, keine angewendete
  Fluid-Gegenreaktion, keine globale FSI-Erhaltung, externe 3D-CFD-
  Messvalidierung oder Unreal-5.8-Native-E2E dieses Pfades.

## P2 Teilfortschritt: hydrostatischer Auftrieb + Eigengewicht + exaktes Potential (29.10.2026)

- Neuer optionaler `P2HydrostaticGravity3D`-Pfad kombiniert den
  hydrostatischen Auftrieb eines ruhenden horizontalen Wasserstandes
  mit der separaten aeusseren Gewichtskraft auf einen bewegten Quader.
  Die Fluid-Gegenreaktion wird ausschliesslich fuer den Auftrieb
  diagnostiziert, nicht fuer das Eigengewicht und nicht auf MAC angewendet.
- Analytische Referenz fuer das exakte hydrostatische Potential bei
  trockener, teilweiser und voller Eintauchung. Die mechanische
  Potentialabweichung bei eingefrorener Kraft wird offengelegt und
  beim Durchqueren einer Teilzellen-Wasserlinie explizit als
  **nicht null** getestet; keine falsche globale Energiefreigabe.
- 24 analytische 3D-Faelle (8^3/12^3, sechs Wasserstaende,
  g=0/9.81), Neutralauftrieb und Fehlerfaelle; Native-Build,
  CTest und SHA256-Teilnachweis in
  `docs/P2_HYDROSTATIC_GRAVITY_PARTIAL.md` und
  `ENGINE TEST STUBE/runs/p2_hydrostatic_gravity_partial.json`.
- **P2-P8 bleiben PARTIAL, ALL_COMPLETE=False.** Offene
  freie-Oberflaechen-/MAC-Gegenreaktion, physikalische Gesamtbilanzen,
  externe CFD-Referenzen und UE5.8-Native-E2E dieses Pfads.

## P2 Teilfortschritt: diskreter hydrostatischer Energiegradient (29.10.2026)

- Neuer opt-in `P2HydrostaticDiscreteGradient3D`: die Auftriebskraft wird
  ueber die tatsaechliche vertikale Starrkoerper-Verschiebung integriert,
  inklusive Nass/Trocken-Uebergang. Der bisherige eingefrorene Kraftschritt
  bleibt als Vergleich unveraendert. Exakte diskrete Starrkoerper-
  Potential-/Kinetikbilanz fuer einen fest vorgegebenen Wasserstand.
- MSVC-Release, komplette 84/84 CTest-Suites, 48 analytisch kontrollierte
  Konfigurationen (8^3/12^3, sechs Wasserstaende, g=0/9.81, +/- v0),
  numerisch unabhaengige Auftriebswegmittelung, Nass/Trocken-Uebergaenge,
  Neutralauftrieb, Zeitumkehr und Negativfaelle. Quantitatives Gate:
  scripts/p2_hydrostatic_discrete_gradient_acceptance.py.
  Nachweise: docs/P2_HYDROSTATIC_DISCRETE_GRADIENT_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_hydrostatic_discrete_gradient_partial.json.
- **P2-P8 bleiben PARTIAL, ALL_COMPLETE=False.** Die Energieabnahme
  betrifft nur den Starrkoerper im eingefrorenen Wasserpotential. Keine
  angewendete MAC/VOF-Gegenreaktion, keine dynamische Wasseroberflaeche,
  keine globale FSI-Energie-/Impulserhaltung, keine externen 3D-CFD-
  Messdaten und kein Unreal-5.8-Native-E2E dieses neuen Pfads.

## P2 Teilfortschritt: geschlossenes 3D-Becken mit hydrostatischer Wasserverdrängung (29.10.2026)

- Opt-in P2ClosedBasinHydrostatic3D: Wasserstand folgt dem
  **konstanten Wasservolumen** und der sich vertikal bewegenden
  Quadergeometrie. Initiale und finale 3D-Cut-Cell-Wasserfraktionen
  werden aus den tatsächlich offenen Zellvolumina rekonstruiert.
- Exakte stückweise Auftriebs-Wegintegration und diskreter Gradient
  schließen die Energie von Starrkörper + **quasistatischem**
  Wasserschwerepotential, einschließlich veränderter Wasserlinie.
- Nachweis: docs/P2_CLOSED_BASIN_HYDROSTATIC_PARTIAL.md,
  scripts/p2_closed_basin_hydrostatic_acceptance.py und
  ENGINE TEST STUBE/runs/p2_closed_basin_hydrostatic_partial.json.
  Die native Suite ist Teil des vollständigen CTest-Gates.
- **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.** Keine MAC/VOF-
  Zeitintegration, kein angewendeter Fluid-Gegenimpuls, keine
  Fluidträgheit oder vollständige FSI-Energie-/Impulsbilanz,
  keine externen 3D-CFD-Messreferenzen und kein neues UE5.8-Native-E2E.

## P2 Teilfortschritt: konservative 3D-Kinematikfluesse im geschlossenen Becken (29.10.2026)

- Neuer opt-in `P2ClosedBasinKinematicFlux3D` erzeugt aus alten/neuen
  Wasserinventaren des geschlossenen Beckens und den zeitgemittelten
  Cut-Cell-Face-Aperturen ein 3D-MAC-Volumenflussfeld U/V/W.
  Der geschlossene Beckenrand bleibt exakt flussfrei.
- Gewichtete Graph-Laplace-Rekonstruktion mit CG, 32 analytisch
  parametrisierte 3D-Faelle auf 8^3/12^3, unabhängige zellweise
  Finite-Volumen-Neuberechnung, negative Eingaben, komplette CTest-
  Suite, Source-/Testprotokoll-SHA256.
- Nachweise: `docs/P2_CLOSED_BASIN_KINEMATIC_FLUX_PARTIAL.md`,
  `scripts/p2_closed_basin_kinematic_flux_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_closed_basin_kinematic_flux_partial.json`.
- Zusatzpruefung (29.10.2026): DSU-Komponentenbilanz vor dem
  Graph-Laplace-CG. Zwei getrennte 3D-Wasserinseln koennen je fuer
  sich konservativ umverteilen; global bilanzierte, aber zwischen
  getrennten Inseln unmoegliche Transfers werden abgelehnt.
  Native MSVC/86 CTest und separater SHA-Teilnachweis erneuert.
- **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.** Dies ist nur eine
  kinematische Flussrekonstruktion zwischen quasistatischen Zustaenden.
  Keine dynamische MAC/VOF-Advektion, keine angewendete FSI-
  Gegenreaktion, keine physikalische Gesamtenergie-/Impulserhaltung,
  keine externen CFD-Messdaten oder Unreal-5.8-Native-E2E.

## P2 Zusatzfortschritt: 3D-Cut-Cell-MAC-Flux-Replay (29.10.2026)

- `P2ClosedBasinFluxReplay3D` wendet die rekonstruierten integrierten
  MAC-Face-Volumina konservativ auf das alte Wasserinventar und die
  neuen Cut-Cell-Kapazitaeten an. Kein Navier-Stokes-/FSI-Schritt.
- 32 3D-Regressionskonfigurationen pruefen die zellweise U/V/W-
  Bilanz gegen den unabhaengig hydrostatisch berechneten Zielzustand.
  Fuenf Negativfaelle lehnen unphysikalische/ungueltige Fluesse ab.
- Belege: `docs/P2_CLOSED_BASIN_KINEMATIC_FLUX_PARTIAL.md`,
  `scripts/p2_closed_basin_kinematic_flux_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_closed_basin_kinematic_flux_partial.json`
  und der vollstaendige 86-Suite-CTest-Log.
- **P2 weiterhin PARTIAL**: kein dynamisches VOF-Upwind,
  keine Fluidtraegheit, keine angewendete Festkoerper-Gegenreaktion,
  keine globale physikalische FSI-Energie-/Impulserhaltung,
  keine externe 3D-CFD-Messvalidierung oder UE5.8-Native-E2E
  dieses neuen Pfads. P3-P8 bleiben ebenfalls PARTIAL.

## P2 Teilfortschritt: dynamisches 3D-Upwind-VOF auf stationaeren Cut-Cells (29.10.2026)

- Neuer opt-in `P2StaticCutCellVof3D` transportiert Wasserfraktionen
  ueber U/V/W-MAC-Faces mit echten stationaeren Cut-Cell-Aperturen.
  First-order donor-cell Upwind, konservatives Zellinventar, strikter
  lokaler Donor-CFL und diskrete Volumendivergenz-Pruefung.
- Native Tests: 8^3/12^3 mit je 40 Zeitschritten, dreiachsige
  divergentenfreie Wirbel, konstante Vollwasser-Invariante,
  unabhaengiger analytischer Donor-Einzelschritt, teilweise
  blockierte Zellvolumina/Face-Aperturen und acht Negativfaelle.
- Nachweise: `docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md`,
  `scripts/p2_static_cut_cell_vof_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json`
  und vollstaendiges MSVC/CTest-Protokoll.
- **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.** Vorgeschriebene
  MAC-Geschwindigkeiten und stationaere Festkoerpergeometrie sind
  noch keine dynamische Navier-Stokes-/FSI-Kopplung. Externe
  3D-CFD-Messvalidierung, globale physikalische Energie-/Impulsbilanz,
  bewegte freie Oberflaechen-Stressgrenze und UE5.8-Native-E2E
  dieses Pfads fehlen.

## P2 Zusatz: 3D-VOF mit temporalem Subcycling (29.10.2026)

- Opt-in P2StaticCutCellVof3D::step_subcycled wiederholt konservative 3D-Upwind-VOF-Transporte mit vorgeschriebenen U/V/W-Feldern und stationaerer Cut-Cell-Geometrie.
- Native 8^3/12^3: Einzelschritt-CFL-Verletzung bei 40-facher Geschwindigkeit, vier Teilsteps gegen separate Referenzaufrufe, Wasserbilanz, Fraktionsgrenzen, Einzelschritt-Paritaet und Negativtests. Vollstaendige CTest-/SHA-Teilabnahme unter docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md.
- **P2-P8 PARTIAL, ALL_COMPLETE=False**: keine Navier-Stokes/FSI-Kopplung, keine stresskonsistente freie Oberflaeche, keine externe CFD-Messvalidierung und kein neuer UE5.8-Native-E2E.

## P2 Teilfortschritt: automatisches 3D-VOF-CFL-Subcycling (29.10.2026)

- P2StaticCutCellVof3D::step_auto_subcycled waehlt die minimale sichere Teilstepzahl aus geometrischem Donor-CFL (Ziel 0.8, max. 1024); unzureichendes Budget wird abgelehnt.
- Zwei native 3D-Gitter (8^3, 12^3), zellweise Paritaet zu manuellem Subcycling, konserviertes Wasservolumen, CFL und Negativfaelle; 87/87 CTest nach Fehlerkorrektur. Separater SHA256-JSON-Nachweis: scripts/p2_static_cut_cell_vof_acceptance.py.
- **P2-P8 PARTIAL, ALL_COMPLETE=False**: nur vorgeschriebene MAC-Geschwindigkeit, stationaere Cut-Cells. Keine dynamische Navier-Stokes/FSI-Gegenreaktion oder externe 3D-CFD-/UE5.8-E2E-Abnahme.
## P2 Teilfortschritt: zeitlich variable MAC-VOF-Velocity-Sequence (29.10.2026)

- Opt-in step_velocity_sequence: vier unterschiedliche vorgeschriebene 3D-MAC-Felder, stationaere Cut-Cells, konservativer Upwind-VOF-Transport mit Einzelstep-Validierung.
- 8^3/12^3 Native-Tests, manuelle Vier-Step-Referenz, Wassererhaltung, Divergenz-/CFL-/NaN-Negativtests und SHA256-Teilnachweis in docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md.
- **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.** Keine dynamische Navier-Stokes-/FSI-Kopplung, keine externe 3D-CFD-Validierung und keine neue UE5.8-Native-E2E.


## P2 Zusatz: nichtuniforme vorgeschriebene MAC-VOF-Zeitfolgen (30.10.2026)

- step_velocity_sequence_adaptive akzeptiert unterschiedliche positive
  Zeitdauern pro externem U/V/W-Feld. Jeder Abschnitt erhaelt sichere
  geometrische CFL-Teilsteps unter einem gemeinsamen Limit von 1024.
- Native 8^3/12^3, manuelle Referenz, Massen-/Fuellgrenzen,
  Negativtests und SHA-JSON-Teilgate.
- P2-P8 PARTIAL, ALL_COMPLETE=False: weiterhin vorgeschriebene
  MAC-Felder, stationaere Cut-Cells, keine physikalische FSI-
  Impuls-/Energieerhaltung oder externe CFD-/UE5.8-Native-E2E-Abnahme.

## P2 Zusatz: konservatives signiertes MAC-Face-Wasserledger (30.10.2026)

- P2StaticCutCellVof3D gibt fuer jeden U/V/W-Face das signierte
  transportierte Wasservolumen in m^3 zurueck. Zeitfolgen und
  Subcycling akkumulieren diese Face-Volumina ueber alle Teilsteps.
- Unabhaengige zellweise Finite-Volumen-Pruefung fuer 8^3/12^3,
  stationaere und fraktionale Cut-Cells, vier Transportmodi,
  beidseitige Fluesse und Nullfluss an geschlossenen Aussenraendern.
  Native CTest, JSON-Teilgate und SHA256 in
  docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Dies ist ein passives
  Wassertransport-Ledger, keine gekoppelte Fluid-Impuls-/Energie-
  integration, keine reale FSI-Gegenreaktion oder UE5.8-Native-E2E.

## P2 Teilfortschritt: passiver 3D-Wasser-/Stofftransport (30.10.2026)

- `P2StaticCutCellVof3D::step_passive_tracer` advectiert einen geloesten
  passiven Stoff [kg/m^3 Wasser] ueber die signierten 3D-MAC-Wasserfluesse
  des konservativen stationaeren Cut-Cell-VOF-Einzelschritts. Separate
  Stoffmassenbilanz, keine Stoffmengen in trockenen/gesperrten Zellen.
- Native MSVC Release: 87/87 CTest PASS, 234710 Assertions im betroffenen
  Test. 8^3/12^3 mit 40 Schritten, analytische 0.06-kg-Donor-Referenz,
  zellweise U/V/W-Stoffledger-Referenz, vier Negativtests und
  fraktionale Cut-Cells. JSON-/SHA-Teilgate:
  scripts/p2_static_cut_cell_vof_acceptance.py.
- **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.** Dies ist kein
  physikalisch voll gekoppelter 3D-Fluid-/Boden-/Thermal-/FSI-Solver,
  keine externe CFD-Messvalidierung und keine neue UE5.8-Native-E2E.

## P2 Teilfortschritt: adaptiver mehrstufiger 3D-VOF-/Tracertransport (30.10.2026)

- step_passive_tracer_sequence_adaptive koppelt den konservativen passiven Stofftransport an jeden echten MAC-VOF-CFL-Teilstep statt an nur aufsummierte Face-Wasserfluesse. Zeitlich variable vorgeschriebene U/V/W-Felder und fraktionale stationaere Cut-Cells getestet.
- MSVC Release 87/87 CTest, 239230 native Assertions; 8^3/12^3 mit je sechs Teilsteps, manuelle Wasser-/Stoff-/Face-Ledger-Referenz und sechs Negativtests. Quell- und CTest-SHA im JSON-Teilgate. Details: docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md.
- **P2-P8 bleiben PARTIAL, ALL_COMPLETE=False.** Keine echte Navier-Stokes-/FSI-Gegenreaktion, keine physikalische Gesamtenergie-/Impulserhaltung, keine externe 3D-CFD-Messvalidierung und kein UE5.8-Native-E2E dieses Pfades.


## P2 Teilfortschritt: signiertes 3D-Tracer-Face-Ledger (30.10.2026)

- P2StaticCutCellVof3D gibt jetzt pro U/V/W-MAC-Face die tatsaechlich
  transportierte signierte passive Stoffmasse [kg] aus. Adaptive Zeitfolgen
  akkumulieren diese Massentransfers aus jedem aktualisierten Donorzustand.
- Native Tests: 8^3 und 12^3, sechs adaptive Schritte mit wechselnden
  vorgeschriebenen Geschwindigkeiten, Vergleich mit separatem Replay,
  zellweise Finite-Volumen-Bilanz aus kumulierten Face-Massen,
  geschlossene Randflaechen und analytische 0.06-kg-Referenz.
  CTest/JSON/Quellhash-Nachweis: scripts/p2_static_cut_cell_vof_acceptance.py.
- **P2-P8 PARTIAL; ALL_COMPLETE=False.** Nur passiver Stofftransport
  bei stationaerer Geometrie und externen MAC-Geschwindigkeiten;
  keine dynamische Fluid-Festkoerper-Gegenreaktion, globale physikalische
  Impuls-/Energiebilanz, externe 3D-CFD-Messvalidierung oder neuer
  UE5.8-Native-E2E fuer diesen Pfad.

## P2 Teilfortschritt: konservative 3D-Fick-Tracerdiffusion (30.10.2026)

- P2CutCellTracerDiffusion3D::step transportiert nach dem vorhandenen
  Upwind-VOF-/Tracer-Einzelschritt geloeste Stoffe ueber benetzte
  Cut-Cell-U/V/W-Flaechen, konservativ und mit separatem signiertem
  Diffusionsledger. Lokale Diffusions-CFL, Positivitaet, Nullfluss-
  Waende und exakte D=0-Paritaet.
- 8^3/12^3 mit je 40 Schritten, analytischer Sechs-Nachbar-
  Diffusionsreferenz, zellweiser 3D-Massenbilanz, fraktionalen
  Cut-Cells und Negativfaellen. Nachweise:
  docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md,
  scripts/p2_static_cut_cell_vof_acceptance.py und
  ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json.
- P2-P8 PARTIAL, ALL_COMPLETE=False. Die Wasserbenetzung ist
  angenaehert; keine dynamische Navier-Stokes-/FSI-Gegenreaktion,
  keine physikalische Gesamtimpuls-/Energiebilanz, externe
  3D-CFD-Messvalidierung oder UE5.8-Native-E2E dieses Pfads.

## P2 Teilfortschritt: diagonal anisotrope 3D-Stoffdiffusion (30.10.2026)

- Opt-in P2CutCellTracerDiffusion3D::step_anisotropic mit drei
  gitterachsenbezogenen Fick-Diffusivitaeten fuer U/V/W-Faces.
  Skalarer Altpfad bleibt bitgleich; getrennte signierte Fluesse,
  lokale CFL und Massenbilanz bleiben erhalten.
- Native Referenz auf 8^3/12^3: analytischer 3D-Sechs-Nachbar-Puls
  (0.988 kg Zentrum, 0.001/0.002/0.003 kg pro Achsen-Nachbar),
  CFL 0.012, zellweises Ledger, Nullachsen und Negativtests.
  Nachweise in docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md und dem
  bestehenden JSON-/CTest-/SHA256-Teilgate.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Nur diagonale numerische
  Diffusion bei stationaeren Cut-Cells und vorgeschriebenen MAC-Feldern.
  Keine gemessene Bodenanisotropie, Navier-Stokes-/FSI-Gegenreaktion,
  globale physikalische Energie-/Impulsbilanz oder neue UE5.8-E2E.


## P2 Teilfortschritt: begrenztes 3D-Diffusions-CFL-Subcycling (30.10.2026)

- Opt-in konservativer 3D-Tracer-Diffusions-Teilstepoperator nach einem
  vorgeschriebenen MAC-Wasser-Advektionsschritt; anisotrope Diagonalwerte,
  konservative Cut-Cell-CFL-Obergrenze und begrenztes 1024-Step-Budget.
- Native 8^3/12^3, analytische Zwei-Schritt-Pulsreferenz (0.15625 kg
  Zentrum, 1 kg gesamt, CFL 0.75), U/V/W-Massenledger, Replay,
  fraktionale Cut-Cells (0.784 kg, 3 Schritte, Residuum <2e-16 kg),
  Negativtests und vorhandene dynamische MAC-Paritaet.
  MSVC Release 87/87 CTest PASS, 270388 native Assertions.
  JSON-/CTest-/Quellhash-Nachweis in
  scripts/p2_static_cut_cell_vof_acceptance.py.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Keine gekoppelte Navier-Stokes-
  Fluid-/Boden-/Thermal-/FSI-Gesamtphysik, keine unabhaengigen 3D-CFD-
  Messdaten oder UE5.8-Native-E2E-Abnahme dieses Pfades.

## P2 Teilfortschritt: zellweise heterogene 3D-Stoffdiffusion (30.10.2026)

- Neuer opt-in Operator P2CutCellTracerDiffusion3D::step_heterogeneous:
  zellweise D_x/D_y/D_z [m2/s] und harmonische Face-Leitwerte an
  Materialgrenzen; Null-D blockiert den Stofffluss. Vorhandene
  3D-MAC/VOF-Advektion bleibt vorgeschrieben, Festkoerper stationaer.
- Unabhaengige 8^3/12^3-Referenz: D_x links=1, Mitte=2, rechts=4,
  dt=0.1 s; Zentrum=0.6 kg, links=2/15 kg, rechts=4/15 kg,
  Gesamtmasse=1 kg, CFL=0.4. Signierte Face-Fluesse und unabhaengige
  zellweise 3D-Massenbilanzen sowie uniforme D-Paritaet getestet.
  Ungueltige Feldgroessen, negative/nichtfinite D und CFL-Verletzung
  werden abgelehnt. Native Release, CTest und SHA256-Teilgate dokumentiert
  in docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md.
- **P2-P8 PARTIAL; ALL_COMPLETE=False.** Keine experimentelle
  Bodenparametrisierung, echte 3D-Navier-Stokes-/FSI-Gegenreaktion,
  Gesamtimpuls-/Energievalidierung oder neue UE5.8-Native-E2E.

## P2 Teilfortschritt: adaptives Subcycling zellweise heterogener 3D-Diffusion (30.10.2026)

- Neuer opt-in Operator P2CutCellTracerDiffusion3D::step_diffusion_subcycled_heterogeneous: nach EINEM vorgeschriebenen 3D-MAC/VOF-Advektionsschritt konservative explizite Diffusions-Substeps mit zellweisen D_x/D_y/D_z, harmonischen Face-Leitwerten und stationaeren Cut-Cell-Aperturen.
- Die exakte lokale Summe der benetzten Face-Leitwerte relativ zum Wasservolumen bestimmt die Substepzahl (CFL-Ziel <=1, Budget maximal 1024); U/V/W-Stoffmassenledger werden aus jedem aktualisierten Substep akkumuliert. Null-D behaelt den bisherigen passiven Advektionszustand exakt bei.
- MSVC Release, 87/87 CTest, 288988 native Assertions im P2-Test. 8^3/12^3: 3 Substeps bei maximaler CFL 2/3, 1 kg Stoffmasse, Zellbilanz-Residuum 5.55e-17 kg. Fraktionale Cut-Cells: 2 Substeps, 8 partielle Zellen, Residuum 1.11e-16 kg. Manuelles Replay und Negativtests. JSON/Quellhash/CTest-Gate: scripts/p2_static_cut_cell_vof_acceptance.py.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Nur numerischer passiver Stofftransport bei vorgeschriebenen MAC-Geschwindigkeiten und statischer Geometrie, keine kalibrierte Bodenchemie, dynamische 3D-Navier-Stokes-/FSI-Kopplung, physikalische Gesamtimpuls-/Energiebilanz, externe CFD-Messvalidierung oder neue Unreal-5.8-Native-E2E.

## P2 Teilfortschritt: robustes harmonisches D an 3D-Materialgrenzen (30.10.2026)

- Numerisch skaliertes harmonisches Mittel fuer heterogene D_x/D_y/D_z
  verhindert falsche Nullfluesse durch subnormale positive Koeffizienten
  sowie Zwischenueberlauf bei grossen endlichen Koeffizienten.
- Native 8^3/12^3: analytische Grenzwerttests fuer 1e-310/1e308
  (2e-10 kg), gleiche 1e-310 (1e-10 kg), gleiche 1e308
  (0.01 kg); Massenerhaltung und Subcycle-Paritaet. Nachweis im
  P2-Static-Cut-Cell-VOF-Teilgate mit CTest- und Quell-SHA256.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Reine numerische Robustheit,
  keine kalibrierte Materialphysik, Navier-Stokes-/FSI-Gesamtvalidierung
  oder neue Unreal-5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: konservative numerische Mischungsdiagnose (30.10.2026)

- step_diffusion_subcycled_heterogeneous bilanziert das diskrete,
  mit benetztem Zellvolumen gewichtete Konzentrationsquadrat
  Q=sum(V_wet*c^2) [kg^2/m^3] unmittelbar vor und nach den
  heterogenen Diffusions-Substeps. Die MAC-Advektion liegt VOR dem
  Anfangswert; die Messung ist daher keine physikalische Energie.
- Fuer symmetrische harmonische Face-Leitwerte, Nullfluss-Waende und
  lokale explizite CFL <= 1 ist Q monoton nicht steigend. Ein Anstieg
  oberhalb der Rundungstoleranz wird als numerischer Fehler abgelehnt.
  Null-D bleibt bitgleich zum bisherigen Transport.
- Native 8^3/12^3: unabhaengige exakte Dreizell-Referenz nach drei
  Schritten: Q_vor=1, Q_nach=20045/59049=0.339463835119985,
  Verlust=39004/59049=0.660536164880015 [kg^2/m^3].
  Fraktionale Cut-Cells: Q 26.5550848 -> 26.501312115821687 (8^3),
  158.4486848 -> 158.21113753694465 (12^3).
  Test: tests/p2_static_cut_cell_vof3d_tests.cpp;
  Teilgate: scripts/p2_static_cut_cell_vof_acceptance.py.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Das ist ein numerischer
  Mischungs-/Monotonienachweis, keine thermodynamische Entropie,
  physikalische Fluid-/FSI-Energiebilanz, externe CFD-Messvalidierung
  oder UE5.8-Native-E2E-Abnahme dieses Transportpfads.

## P2 Teilfortschritt: diskrete Euler-Mischungsidentitaet (30.10.2026)

- Fuer heterogene 3D-Cut-Cell-Tracerdiffusion wird der numerische
  Q-Verlust unabhaengig aus Face-Gradienten-Drive und
  expliziter Euler-Korrektur pro Substep bilanziert.
- 8^3/12^3 Dreizell-Referenz sowie fraktionale Cut-Cells;
  Quell-/CTest-Hashes und CTest-Gesamtlauf sind Gate-Pflicht.
  Details: docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md.
- Dies ist **nur eine numerische Teilabnahme**. P2 bleibt PARTIAL;
  echte dynamische 3D-FSI, globale physikalische Energie/Impuls,
  externe CFD-Referenzen und UE5.8-Native-E2E bleiben offen.

## P2 partial validation: independent 3D Neumann eigenmode (30.10.2026)

- A three-axis cosine concentration mode in a closed 1 m cube is diffused
  with zero prescribed MAC velocity, uniform D=0.03 m2/s, and T=0.2 s.
  The exact continuum reference is exp(-3*pi^2*D*T). The independently
  computed discrete finite-volume reference is
  [1-12*D*dt/dx^2*sin^2(pi/(2*n))]^steps.
- Three real native grids (8^3/12^3/16^3), with 4/9/16 explicit time
  steps, verify pointwise 3D eigenmode preservation, nonnegative
  concentrations, solute mass 2 kg, the diffusion CFL and monotone
  quadratic mixing. The separate Python gate independently recomputes
  both eigenvalues and verifies the second-order grid-refinement trend.
- Verified MSVC Release + 87/87 CTest PASS, 301844 native assertions,
  210/210 quantitative Python partial checks. Measured mode gains:
  n=8: 0.83581949087047847; n=12: 0.83661024489195246;
  n=16: 0.8368837601361514. Continuum gain: 0.83723299402457019.
  Absolute amplitude errors: 0.00141350315409, 0.000622749132618,
  0.000349233888419; 3D diffusion CFL 0.576 and total mass 2 kg.
  A 1e-9 m corner solid satisfies the geometry API's mandatory in-domain
  box without measurably changing the unit-cube Neumann stencil.
  Reproducible report: ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json,
  CTest log p2_static_cut_cell_vof_full_ctest_20261029.txt, source SHA256.
- This is a manufactured/analytic *passive solute diffusion* benchmark.
  It is NOT independent measured 3D CFD, dynamic Navier-Stokes,
  physical FSI energy/momentum, water/soil calibration or UE5.8 E2E.
  P2-P8 remain PARTIAL; ALL_COMPLETE=False.

## P2 partial: diffusion-connected 3D compartment mass audit (30.10.2026)

- Optional heterogeneous 3D cut-cell tracer subcycling now constructs
  connected components from *positive actual wet-face conductance* using
  union-find. Every compartment's dissolved mass is audited before and
  after the diffusion substeps. Zero-diffusivity material interfaces,
  dry cells, and sealed faces cannot transfer solute between components.
- Independent 8^3 and 12^3 native tests construct three 3D water
  compartments separated by a zero-X-diffusivity plane, with nontrivial
  Y/Z diffusion and a fixed solid voxel. The Python acceptance gate
  recomputes each compartment's initial mass from its analytic
  concentration field, independently of the C++ audit. Signed fluxes
  across both sealed X-face planes must remain exactly zero.
- This is a local conservation check for *passive diffusion after
  prescribed-MAC advection*, not global fluid/solid mass, physical
  energy or impulse. No moving body, measured CFD/soil reference,
  or new Unreal 5.8 Native-E2E. P2-P8 remain PARTIAL.
- Verified MSVC Release, 87/87 CTest, 302276 native assertions and
  219/219 quantitative partial checks. Compartment masses (kg):
  8^3: 281.79 / 207.68 / 991.68, maximum residual 3.41e-13 kg;
  12^3: 669.95 / 487.44 / 5286.96, maximum residual 3.64e-12 kg.
  Three stable diffusion substeps in both cases. Source and CTest
  SHA256 are recorded in p2_static_cut_cell_vof_partial.json.


## P2 Teilfortschritt: fraktionale heterogene Zwei-Volumen-Relaxation (30.10.2026)

- Unabhaengiger diskreter 3D-Eigenmode-Test der konservativen
  Cut-Cell-Stoffdiffusion: ungleiche Wasserzellvolumina 0.25/1 m3,
  harmonischer Materialuebergang D=(0.1,0.4) m2/s, exakter
  Relaxationsfaktor 0.8 pro Schritt und Gleichgewicht 0.8 kg/m3.
- Nach 24 Schritten: Konzentrationen 0.81511157274518276 /
  0.79622210681370409 kg/m3, 1 kg erhalten, CFL 0.16;
  8^3 und 12^3. MSVC Release 87/87 CTest, 302618 Assertions,
  228/228 quantitative Teilchecks mit Quell-/CTest-Hashes.
- docs/P2_STATIC_CUT_CELL_VOF_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json.
  **P2-P8 weiterhin INCOMPLETE**: keine externe 3D-CFD-Messvalidierung,
  keine dynamische Gesamtimpuls-/Energieerhaltung oder UE5.8-Native-E2E
  dieses Tracer-Pfades.

## P2 Teilfortschritt: transiente 3D-Wandscherung mit spektraler Referenz (30.10.2026)

- Zusaetzlicher nativer MSVC-Test fuer 40 reale 3D-MAC-Viskositaetszeitschritte mit vorgeschriebener tangentialer Festkoerper-Ghostgeschwindigkeit. Zwei Aufloesungen (8x8x5 und 12x12x7 aktive Wasserzellen in y/z/x) und gemischte feste/wasserfreie Randbedingungen.
- Unabhaengige diskrete Tensorprodukt-Sinusloesung prueft alle y-Faces und die 3D-Quergradienten; x/z bleiben null. Reproduzierbarer separater Build, kompletter bestehender CTest-Lauf und Python-Gate mit SHA256. Siehe docs/P2_DRIVEN_WALL_SPECTRAL_PARTIAL.md.
- P2-P8 bleiben PARTIAL: statische Geometrie, kein bewegter Koerper/Impuls-Gegenreaktion, keine physikalische Gesamtenergie, keine externen 3D-CFD-Messdaten und kein neuer UE5.8-Native-E2E.

## P2 Teilfortschritt: fraktionale dreiachsige 3D-Druckarbeit (30.10.2026)

- Eigenstaendiger nativer Test des vorhandenen Cut-Cell-Drucklastoperators
  bei simultaner Subzellen-Translation eines 2.5^3-Quaders entlang x/y/z.
  Zellmittel-Druckgradienten (+12,-7,+5) Pa/Zelle liefern analytisch
  F=(-150,+87.5,-62.5)*dx^2 N und W=-29.125*dx^3 J.
- Zwei 3D-Gitter (8^3, dx=0.1 m; 12^3, dx=0.125 m), beide
  Bewegungsrichtungen und drei Einzelachsen; Gauge-Verschiebung +500 Pa,
  Druck-/Geometrie-Negativfaelle, diagnostischer Gegenimpuls und
  Druckarbeitsparitaet. Native MSVC-Release 355 Assertions und
  87/87 bestehende CTest-Suites bestanden.
- Test, Build, Python-Gate und Quellen-/CTest-SHA256-Nachweis:
  docs/P2_FRACTIONAL_PRESSURE_WORK_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_fractional_pressure_work_partial.json.
- P2 bleibt INCOMPLETE, ebenso P3-P8: nur vorgeschriebene Geometrie,
  keine nachprojizierte bidirektionale FSI, keine physikalische globale
  Energie-/Impulserhaltung, keine externen CFD-Messdaten und keine neue
  UE5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: Nichtlineares 3D-Druckmoment und Referenzpunkt-Impuls (30.10.2026)

- Neuer optionaler, isolierter C++17-Diagnoseadapter `P2PressureWrenchReference3D` fuer diskrete Druckkraft und Drehmoment um einen frei waehlbaren Referenzpunkt: `M_ref=M_center+(center-ref) x F`; linearer und angularer Gegenimpuls sind nur Buchung, **keine** Fluid-FSI-Rueckwirkung.
- Fraktionale 3D-Box und nichtlineares Druckfeld mit echtem intrinsischem Drehmoment: native MSVC-Tests auf 8^3/12^3, beide Bewegungsrichtungen, Gauge-Invarianz, geometrische Druckarbeit, Ablehnung der Translation-only-Dynamik bei Drehmoment.
- Unabhaengige Python-Referenz und SHA256: `scripts/p2_pressure_wrench_reference_acceptance.py`, `docs/P2_PRESSURE_WRENCH_REFERENCE_PARTIAL.md`, `ENGINE TEST STUBE/runs/p2_pressure_wrench_reference_partial.json`. Die volle CTest-Regression muss weiter gruen bleiben.
- **P2 bleibt INCOMPLETE**: kein orientierter rotierender Koerper, keine dynamische zweigerichtete MAC-Impulsrueckgabe, keine externe CFD-Messvalidierung und keine neue UE5.8-Native-E2E-Abnahme. P3-P8 bleiben ebenfalls offen.

## P2 Teilfortschritt: Druckwrench auf gefrorene Starrkoerper-ODE (30.10.2026)

- Neuer optionaler C++17-Adapter `P2FrozenWrenchRigidStep3D` koppelt den
  nichtlinearen, fraktionalen 3D-Druckwrench an den vorhandenen
  RigidPressureResponse3D-ODE-Schritt mit Translation **und** Winkelgeschwindigkeit.
  Referenzpunktverschiebung, Anfangs-Koerpermitte, Diagonaltraegheit und
  die Differenz zur vorgeschriebenen Boxbewegung werden explizit geprueft.
- 8^3/12^3, beide Bewegungsrichtungen, analytische unabhaengige
  Druckflaechen-/ODE-Referenzen, Gauge- und Referenzpunktinvarianz,
  176 native Assertions, volle 87/87 CTest-Suite.
- Nachweise: `docs/P2_FROZEN_WRENCH_RIGID_PARTIAL.md`,
  `scripts/p2_frozen_wrench_rigid_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_frozen_wrench_rigid_partial.json`.
- **P2-P8 weiterhin INCOMPLETE**: gefrorene Last, vorgegebene Box statt
  aktualisierter rotierender Geometrie, kein MAC-Gegenimpuls, keine
  globale Fluidenergie, keine externe 3D-CFD-Messvalidierung und
  keine Unreal-5.8-Native-E2E-Abnahme dieses Pfades.

## P2 Teilfortschritt: angewendeter 6-DOF-MAC-Gegenimpuls (30.10.2026)

- Opt-in C++17-Adapter `P2FrozenWrenchMacReaction3D`: der echte
  nichtlineare fraktionale Druckwrench wird erst in die gefrorene
  Starrkoerper-ODE integriert und sein negativer linearer UND angularer
  Impuls dann auf benachbarte nasse MAC-Innenflaechen angewendet.
- 6x6 diagonal skalierte Minimum-Norm-Nebenbedingung, Rangdefizienz-
  Ablehnung und transaktionales Preflight vor dem Schreiben; aussen,
  trocken und festkoerpernormal bleiben unveraendert.
- Native 8^3/12^3, beide Boxbewegungsrichtungen und ruhende/vorbelegte
  Face-Felder: 34854 Assertions PASS. Volle bestehende MSVC-Release-
  Regression: 87/87 CTest PASS. Unabhaengige Python-6x6-Referenz und
  Source-/Binaer-/CTest-SHA256-Teilgate:
  `scripts/p2_frozen_wrench_mac_reaction_acceptance.py`;
  `docs/P2_FROZEN_WRENCH_MAC_REACTION_PARTIAL.md`;
  `ENGINE TEST STUBE/runs/p2_frozen_wrench_mac_reaction_partial.json`.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False**: nur diskrete staggered-
  Face-Impulsbilanz vor der Druckprojektion, statische Koerpermaske,
  eingefrorene Drucklast, keine echte orientierte Rotationsgeometrie,
  keine globale physikalische FSI-Energie-/Impulserhaltung, keine
  unabhaengige 3D-CFD-Messvalidierung oder Unreal-5.8-Native-E2E.


## P2 Teilfortschritt: 6-DOF-MAC-Gegenimpuls mit Druck-Nachprojektion (30.10.2026)

- Bestehender eingefrorener Druckwrench-Gegenimpuls wird in acht nativen
  3D-Fällen wirklich mit `MacProjection3D::project(dt,3000)` nachprojiziert.
  Separates Impuls- und Energieaudit rekonstruiert die Projektionsänderung
  auf allen MAC-Faces, unabhängig berechnete Zell-Divergenz und
  `J_total = J_frozen + J_projection` um die alte Körpermitte.
- 87/87 MSVC-Release-CTest-Suites PASS; 64.310 Assertions im erweiterten
  Native-Test; Divergenz nach Projektion etwa 1e-10 1/s. Das
  Python-Gate ergänzt die acht Szenarien um numerische
  Projektions-/Korrekturchecks und Quellen-/Binär-/Log-SHA256.
- **P2-P8 weiterhin INCOMPLETE, ALL_COMPLETE=False**: Die
  Nachprojektion verändert die zuvor geschlossene Face-Impulsbilanz,
  und korrigiert auch inkompatible anfängliche Randgeschwindigkeiten.
  Kein physikalischer FSI-Gesamtimpuls-/Energiebeweis, keine rotierende
  Geometrie, keine externen 3D-CFD-Messdaten, kein neuer UE5.8-Native-E2E.
  Details: `docs/P2_FROZEN_WRENCH_MAC_REACTION_PARTIAL.md`.

## P2 Teilfortschritt: diskrete Druckgradientenbilanz nach MAC-Nachprojektion (30.10.2026)

- Read-only C++17-Adapter fuer den Nachprojektions-Impuls und die exakte
  Druckgradienten-Arbeit/Euler-Quadratik der staggered MAC-Face-Proxies.
- Unabhaengige analytische 3D-Einzell-Zell-Referenz mit sechs Wasser-Luft-
  Faces und 2x2x2-Wasserwuerfel fuer Wasser-Wasser-/Wasser-Luft-Kopplung.
- Tests: tests/p2_projection_gradient_budget3d_tests.cpp;
  scripts/p2_projection_gradient_budget_acceptance.py.
- Nachweis: ENGINE TEST STUBE/runs/p2_projection_gradient_budget_partial.json;
  docs/P2_PROJECTION_GRADIENT_BUDGET_PARTIAL.md.
- **P2 bleibt INCOMPLETE**: Die Gradientenbilanz erklaert nur den
  numerischen Nachprojektionsimpuls. Physikalische Wandreaktion,
  globale FSI-Impuls-/Energieerhaltung, bewegte rotierende Geometrie,
  externe CFD-Messdaten und UE5.8-Native-E2E sind weiterhin offen.


## P2 Teilfortschritt: MAC-Projektions-Wand-Clamp-Budget (30.10.2026)

- Der `ProjectionGradientBudgetAudit3D` bilanziert jetzt getrennt
  die durch `MacProjection3D::project` erzwungene Nullgeschwindigkeit
  an Festkörper- und Außenfaces (lineare/angulare Face-Impulse,
  Energie-Proxy, Face-Zahlen und Restgeschwindigkeit).
- Zwei native 3D-Gitter (8³, 12³) mit Wasser/Festkörper-, Festkörper/Luft-
  und Außenface prüfen unabhängige analytische `delta_p`, `delta_L`
  und `delta_E`-Referenzen. Reproduktion und Hashbelege:
  `docs/P2_PROJECTION_WALL_CLAMP_BUDGET_PARTIAL.md`,
  `scripts/p2_projection_wall_clamp_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_projection_wall_clamp_partial.json`.
- **P2 bleibt INCOMPLETE:** Dies ist ein numerisches MAC-Face-Budget,
  keine physikalische Wandkraft-/FSI-Impuls- oder Gesamtenergiebilanz.
  Bewegliche Geometrie, externe 3D-CFD-Daten und UE5.8-Native-E2E
  dieser Erweiterung fehlen. P3–P8 ebenfalls offen.

## P2 Teilfortschritt: vollstaendige MAC-Face-Projektionspartition (31.10.2026)

- Neuer read-only C++17-Adapter `ProjectionFaceClosureAudit3D` summiert
  alle aktiven staggered MAC-Face-Impuls- und Energie-Proxies unabhaengig
  von der Druckgradient-/Solid-/Aussen-Clamp-Zerlegung. Luft/Luft-Faces
  werden separat gezaehlt und auf unveraenderte Geschwindigkeit geprueft.
- Unabhaengige analytische Fuenf-Face-Poisson-Referenz mit einem
  Solid-Nachbarn, 8^3/12^3, drei absichtlichen Fehler-Injektionen.
  Native MSVC-Release-Tests: 82 Assertions PASS; 88/88 bestehende CTest PASS.
  Maximaler Partitionsrest 1.39e-17, Gradientengeschwindigkeitsrest
  5.21e-18 m/s.
- Nachweise: docs/P2_PROJECTION_FACE_CLOSURE_PARTIAL.md;
  scripts/p2_projection_face_closure_acceptance.py;
  ENGINE TEST STUBE/runs/p2_projection_face_closure_partial.json.
- **P2 bleibt INCOMPLETE, P3-P8 ebenfalls; ALL_COMPLETE=False.**
  Numerische Face-Proxies sind keine globale physikalische
  FSI-Impuls-/Energieerhaltung. Rotierende Geometrie, externe 3D-CFD-
  Messdaten und Unreal-5.8-Native-E2E dieses Pfades bleiben offen.

## P2 Teilfortschritt: transaktionale, residualgepruefte 3D-MAC-Projektion (31.10.2026)

- Neue opt-in `MacProjection3D::project_verified`: vollstaendige lokale
  Kandidatenkopie, Endlichkeitspruefung fuer Druck/alle Face-Vektoren,
  tatsaechliche Divergenz aller nassen 3D-Zellen als Akzeptanzgrenze;
  keine Mutation des Originalzustands bei Unterkonvergenz.
  Die bestehende `project()`-API bleibt unveraendert und ungeprueft.
- Unabhaengige analytische Zwei-Wasserzellen-Poisson-Referenz
  mit fuenf Wasser-Luft-Grenzen pro Zelle: p=(-R/12,+R/12),
  u_innen=0.10 m/s. 8^3/12^3, jeweils ein bewusst zu kurzer
  und ein konvergierter Lauf; 1-Zellen-Referenz mit exakt
  einer Iteration; Null-/NaN-Toleranz, invalides dt, vollstaendige
  bitgenaue 3D-Rollback-Pruefung.
- MSVC-Release, 89/89 CTest-Suites und 38360 native Assertions PASS.
  Nachweis: docs/P2_VERIFIED_MAC_PROJECTION_PARTIAL.md,
  scripts/p2_verified_mac_projection_acceptance.py,
  ENGINE TEST STUBE/runs/p2_verified_mac_projection_partial.json.
- **P2-P8 weiterhin INCOMPLETE; ALL_COMPLETE=False.** Keine
  dynamische rotierende Zweiwege-FSI, keine globale physikalische
  Impuls-/Energieerhaltung, keine unabhaengigen CFD-Messdaten und
  kein Unreal-5.8-Native-E2E dieses neuen Pfades.

## P2 Teilfortschritt: transaktionaler Live-MAC/VOF-Step mit verifizierter Projektion (31.10.2026)

- 'CoupledMacWater3D::step_verified' nutzt die residualgepruefte
  'MacProjection3D::project_verified' in jedem adaptiven Substep.
  Wasserfraktionen, Druck, Geschwindigkeiten, Diagnostik und
  Transportbenachrichtigungen bleiben bei numerischem Fehler unveraendert.
  Transportereignisse werden erst nach erfolgreichem Commit zugestellt.
- Native Tests auf 8^3/12^3-Gittern mit 7 Wasserzellen und Solid-Grenze:
  Unterkonvergenz-Rollback, Legacy-Paritaet, Massenbilanz,
  Listener-Commit-Reihenfolge und Parameterfehler. Details:
  'docs/P2_VERIFIED_COUPLED_MAC_VOF_PARTIAL.md'.
- Nachweis: 'scripts/p2_verified_coupled_acceptance.py' und
  'ENGINE TEST STUBE/runs/p2_verified_coupled_partial.json'.
- **P2-P8 weiterhin INCOMPLETE, ALL_COMPLETE=False.** Keine dynamische
  rotierende FSI, keine globale physikalische Impuls-/Energiebilanz,
  keine unabhaengigen CFD-Messdaten und kein UE5.8 Native-E2E dieses Pfades.

## P2 Teilfortschritt: residualgepruefte Nachprojektion auf finaler VOF-Maske (31.10.2026)

- Konkrete Luecke im bisherigen 'step_verified' reproduziert: Druckprojektion
  vor VOF-Transport laesst neu benetzte Zellen ohne finale
  Divergenzabnahme. Die alte Projektion ergab im finalen Nassmasken-Audit
  2.0306013986 s^-1 auf 8^3 und 12^3.
- Neue opt-in 'step_verified_reprojected': zweiter verifizierter
  MAC-Drucksolve nach jedem VOF-Transport, synchronisierte finale
  Nass-/Solidmaske, transaktionaler Rollback und verzoegerte Listener,
  konservative Massen-/VOF-Bereichspruefung vor Commit.
- Read-only 'current_wet_divergence_s_inv' prueft die tatsaechliche
  aktuelle Wasserbelegung statt der veralteten MAC-Maske.
- Native Reproduktion: 7 kg Masse, 25 benetzte Zellen, 19 Events,
  Divergenz nach finaler Projektion 1.3005135e-11 s^-1.
  Nachweis: docs/P2_POST_TRANSPORT_PROJECTION_PARTIAL.md,
  scripts/p2_post_transport_acceptance.py,
  ENGINE TEST STUBE/runs/p2_post_transport_partial.json.
- **P2-P8 weiterhin INCOMPLETE; ALL_COMPLETE=False.**
  Keine rotierende FSI, keine physikalische Gesamtimpuls-/Energiebilanz,
  keine unabhaengige CFD-Messvalidierung und kein neuer UE5.8 Native-E2E.

## P2 Teilfortschritt: diskrete 3D-VOF-Gravitationspotenzialbilanz (31.10.2026)

- Im bestehenden CoupledMacWater3D ist die Gravitationspotenzialdiagnose
  fuer Wasser-VOF-Zellmittelpunkte und eine pro Transfer integrierte
  Delta-Energie-Bilanz ueber alle adaptiven Substeps hinzugekommen.
- Unabhaengige Native-Pruefung: 8^3 und 12^3, je 7 kg Wasser,
  19 reale VOF-Transfers, Delta-E_pot=-0.11844443076921887 J,
  separate Transport-Event-Bilanz identisch, Unterkonvergenz-Rollback.
  MSVC Release, 92/92 CTest und 72 native Assertions PASS.
- Teil-Gate: scripts/p2_vof_gravity_ledger_acceptance.py,
  ENGINE TEST STUBE/runs/p2_vof_gravity_ledger_partial.json,
  docs/P2_VOF_GRAVITY_LEDGER_PARTIAL.md.
- P2-P8 INCOMPLETE, ALL_COMPLETE=False. Nur diskrete VOF-Potenzialenergie,
  keine kinetische/viskose/Druckarbeit-/FSI-Gesamtenergiebilanz,
  keine externen CFD-Messdaten und kein neues UE5.8 Native-E2E.
## P2 Teilfortschritt: MAC-Face-Energie nach Solver-Stufen (31.10.2026)

- Neue opt-in Diagnose `enable_face_energy_stages(true)` im live 3D-MAC/VOF-Solver: getrennte Face-Quadratsummen-Aenderungen aus Advektion, Viskositaet, Gravitation, Druckprojektion, VOF-Transport und finaler Nachprojektion. Standardmaessig deaktiviert.
- 8^3/12^3, unabhaengige Drei-Face-Gravitationsreferenz 0.014435415 J, gemessen 0.014435415000000002 J; zweite Projektion -0.000041185284665097924 J, algebraischer Abschlussrest 0. MSVC Release, 93/93 CTest, 4524 native Assertions.
- Tests: transaktionaler Rollback, deaktivierte Diagnose mit bitgenauer VOF-/Druck-Paritaet. Details: docs/P2_FACE_ENERGY_STAGES_PARTIAL.md, scripts/p2_face_energy_stages_acceptance.py, ENGINE TEST STUBE/runs/p2_face_energy_stages_partial.json.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Der Face-Quadratsummenwert ist keine VOF-gewichtete physikalische kinetische Energie; keine globale FSI-Energie-/Impulserhaltung, externen CFD-Messdaten oder neue Unreal-5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: VOF-massengewichtetes 3D-Mechanikinventar (31.10.2026)

- Neue read-only Funktion `vof_mechanical_inventory3d` und
  `CoupledMacWater3D::water_mechanical_inventory()` liefern Masse,
  drei Impulskomponenten, zellzentrierte kinetische Energie,
  Gravitationspotenzial und deren Summe. Trockene MAC-Faces werden
  ausgeschlossen; die Geschwindigkeiten sind Zellmittelpunkt-Mittelwerte.
- MSVC Release und 94/94 CTest PASS. Zwei 3D-Raster 8^3/12^3 mit
  unabhaengiger Zwei-Zellen-Referenz: 1 kg, Impuls
  (-0.25,-0.25,-1.375) kg m/s, E_kin=2.53125 J,
  E_pot=4.65975 J, E_sum=7.191 J. Native 62 Assertions;
  Live-MAC/VOF-Snapshots, ungueltige Eingaben und Rollback geprueft.
- Reproduktion: scripts/p2_vof_mechanical_inventory_full_build.cmd;
  scripts/p2_vof_mechanical_inventory_acceptance.py; Details:
  docs/P2_VOF_MECHANICAL_INVENTORY_PARTIAL.md und
  ENGINE TEST STUBE/runs/p2_vof_mechanical_inventory_partial.json.
- **P2-P8 INCOMPLETE, ALL_COMPLETE=False.** Die Zellmittelpunktenergie
  ist eine physikalisch motivierte Naeherung, keine exakte
  VOF-/MAC-Gesamtenergie, kein konservatives dynamisches FSI,
  keine externe CFD-Messvalidierung oder UE5.8-Native-E2E.

## P2 Teilfortschritt: mechanische Inventardifferenz gegen VOF-Transferbuchung (31.10.2026)

- Neue read-only `vof_mechanical_delta_audit3d`: Delta-Masse, Delta-Impuls,
  Delta-KE, Delta-Potenzial und mechanische Differenz zweier
  VOF-gewichteter Wasserinventare. Unabhaengige VOF-Transport-Potenzialbuchung
  liefert einen expliziten algebraischen Abschlussrest.
- Analytischer 0.125-kg-Aufwaertstransfer ueber 0.3 m:
  Delta-E_pot=+0.367875 J, Delta-E_kin=-0.015625 J,
  Delta-E_mech=+0.35225 J. Falsche Buchung und nichtfinite Werte
  werden nachweislich erkannt. 8^3/12^3 plus echte verifizierte
  Live-MAC/VOF-Schritte: Transport-Delta-E_pot=-0.00962361 J,
  Bilanzrest <1e-10 J; 94/94 CTest und 98 native Assertions PASS.
- Gate: scripts/p2_vof_mechanical_inventory_acceptance.py;
  Report: ENGINE TEST STUBE/runs/p2_vof_mechanical_inventory_partial.json;
  Dokumentation: docs/P2_VOF_MECHANICAL_INVENTORY_PARTIAL.md.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Der Nicht-Transport-Anteil
  ist keine physikalisch geschlossene Druck-/Viskositaets-/FSI-Energie.
  Bewegte Zweiwege-FSI, gemessene CFD-Referenzen und UE5.8 Native-E2E
  fuer diesen Pfad fehlen weiterhin.

## P2 Teilfortschritt: VOF-gewichteter Drehimpuls und Traegheit (31.10.2026)

- Neuer read-only 3D-MAC/VOF-Adapter fuer Zellmittelpunkt-Drehimpuls um
  einen frei waehlbaren Ursprung, Massenschwerpunkt, linearen Impuls,
  Punktmassen-Traegheitstensor und Steiner-Verschiebung. Kein Fluid-FSI.
- Zwei analytische 8^3/12^3-Referenzen: 1 kg, Schwerpunkt (0.475 m)^3,
  L=(-0.73125,0.50625,0.225) kg m^2/s, I_COM-Diagonale 0.03375,
  Nebendiagonale -0.016875 kg m^2; Delta-L durch 0.125-kg-Transfer
  (-0.184375,0.021875,0.1625) kg m^2/s.
- Native Tests einschliesslich Ursprungstransformation, trockener Faces,
  Fehlerfaellen und tatsaechlich konvergierter Druckprojektion.
  Nachweis: docs/P2_VOF_ANGULAR_INVENTORY_PARTIAL.md,
  scripts/p2_vof_angular_acceptance.py und
  ENGINE TEST STUBE/runs/p2_vof_angular_partial.json.
- P2-P8 weiterhin INCOMPLETE; ALL_COMPLETE=False. Kein konservatives
  rotierendes Zweiwege-FSI, keine externe 3D-CFD-Messvalidierung,
  keine globale Impuls-/Energieerhaltung und kein UE5.8 Native-E2E.

## P2 Teilfortschritt: gleichfoermige Voxel-Eigentraegheit (31.10.2026)

- Neuer read-only `vof_uniform_cell_inertia3d`-Adapter: zu der
  zellzentrierten Punktmassen-Traegheit kommt die exakte intrinsische
  kubische Eigentraegheit M*dx^2/6 auf jeder Tensor-Diagonale hinzu.
  Voraussetzung: uniformer Fuellgrad innerhalb jeder Zelle, keine
  Rekonstruktion der tatsaechlichen partiellen Wasseroberflaeche.
- Unabhaengige 2x2x2-Gauss-Volumenquadratur, zwei asymmetrische 8^3/12^3
  Felder und ein geometrisch identischer 0,2-m-Wuerfel auf 2^3/4^3/8^3
  Zellen; Schwerpunkt-/Ursprungstensoren und Negativfaelle.
- Nachweis: docs/P2_VOF_UNIFORM_CELL_INERTIA_PARTIAL.md,
  scripts/p2_vof_uniform_cell_acceptance.py,
  ENGINE TEST STUBE/runs/p2_vof_uniform_cell_partial.json.
- **P2-P8 bleiben INCOMPLETE; ALL_COMPLETE=False.** Keine echte
  Unterzellen-VOF-Geometrie, keine rotierende Fluid-Festkoerper-
  Impulsrueckwirkung, keine externen CFD-Messdaten und kein neuer
  UE5.8-Native-E2E-Nachweis.

## P2 Teilfortschritt: affiner MAC-VOF-Eigendrehimpuls (31.10.2026)

- Neuer read-only Adapter vof_affine_spin_inventory3d: MAC-Face-Gradienten
  rekonstruieren lokale Wirbelstaerke, und der fehlende Unterzellen-
  Drehimpuls M*dx^2/12*curl(v) ergaenzt die Zellmittelpunktbilanz.
- Analytischer Starrkoerper-Rotationswuerfel (0.4 m, 64 kg) auf
  8^3/12^3/16^3: I_COM=1.706666666666667 kg m^2,
  L_COM,z=2.218666666666667 kg m^2/s; unabh. Gauss-Quadratur
  fuer asymmetrische Teilfuellungen 8^3/12^3.
- MSVC Release 97/97 CTest, 112 native Assertions PASS.
  Nachweis: docs/P2_VOF_AFFINE_SPIN_PARTIAL.md,
  scripts/p2_vof_affine_spin_acceptance.py und
  ENGINE TEST STUBE/runs/p2_vof_affine_spin_partial.json.
- P2-P8 bleiben INCOMPLETE; ALL_COMPLETE=False. Nur uniforme
  Teilzellendichte und affiner Geschwindigkeitsstencil, keine
  rekonstruierte VOF-Freiflaeche, keine konservative Zeitentwicklung,
  keine bewegte Zweiwege-FSI oder neue UE5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: achsparallele VOF-PLIC-Geometriemomente (31.10.2026)

- Neuer read-only Adapter p2_axis_plic_geometry3d: exakt geschnittene
  rechteckige Teilzellen mit vorgegebener Schnittachse und -seite,
  Wasser-Masse, Schwerpunkt und Traegheitstensor um Ursprung/COM.
- Analytische Halbzelle: 4 kg, Schwerpunkt x=0.05 m (invertiert
  x=0.15 m), Ixx=0.0266666667, Iyy=Izz=0.0166666667 kg m^2.
  8^3/12^3 Vollwuerfel und unabhaengige Gauss-Quadratur asymmetrischer
  teilgefuellter 3D-Volumina mit Ursprungstransformation.
- MSVC Release 98/98 CTest und 125 native Assertions nach finalem
  Rebuild PASS; neues Teil-Gate 33/33, P1-Baseline 33/33 und
  P2-MVP-Baseline 30/30. Nachweis: docs/P2_AXIS_PLIC_GEOMETRY_PARTIAL.md,
  scripts/p2_axis_plic_acceptance.py,
  ENGINE TEST STUBE/runs/p2_axis_plic_partial.json.
- **P2-P8 bleiben INCOMPLETE; ALL_COMPLETE=False.** Keine beliebig
  orientierten PLIC-Normalen, keine Live-VOF-Transportintegration,
  keine bewegte FSI, keine externe CFD-Messvalidierung oder neue
  Unreal-5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: oblique PLIC-Momente (31.10.2026)

- Neuer read-only Adapter `p2_oblique_plic_geometry3d` fuer beliebig
  orientierte, **vorgegebene** planare Schnitte einer 3D-Wasserzelle:
  biseziertes Zielvolumen, polygonales Clipping und exakte Tetraeder-
  Volumen-, Schwerpunkt- und Traegheitsmomente inklusive Nebendiagonalen.
- Unabhaengige analytische Tetraeder- und Dreiecksprisma-Referenzen,
  sechs achsparallele Kreuzvergleiche und asymmetrische 8^3/12^3-Felder.
  Nachweis: docs/P2_OBLIQUE_PLIC_GEOMETRY_PARTIAL.md,
  scripts/p2_oblique_plic_acceptance.py und
  ENGINE TEST STUBE/runs/p2_oblique_plic_partial.json.
- **P2-P8 bleiben INCOMPLETE; ALL_COMPLETE=False.** Die Normale ist
  vorgeschrieben, nicht rekonstruiert; keine Grenzflaechenadvektion,
  bewegte Zweiwege-FSI, externe 3D-CFD-Messdaten, globale Energie-
  /Impulsabnahme oder neues UE5.8 Native-E2E.

## P2 Teilfortschritt: rekonstruierte lokale VOF-Normalen (31.10.2026)

- Neuer read-only Adapter `p2_reconstruct_vof_normals3d` bestimmt
  orientierte Oberflaechennormalen aus benachbarten 3D-VOF-Zellanteilen,
  mit einseitigen Differenzen an Rand-/Solidzellen. Der Adapter
  `p2_vof_reconstructed_plic_geometry3d` fuehrt die Normalen mit
  dem vorhandenen obliquen PLIC-Geometriemomentintegrator zusammen.
- MSVC Release und **100/100 CTest PASS**, 13.065 native Assertions.
  Analytisch affine 8³-/12³-Gradienten exakt innerhalb 2e-13;
  scharfer synthetischer 12³-Ebenenschnitt mit mittlerer
  Normalen-Richtungsuebereinstimmung 0,978487 und minimal 0,844322;
  Solid-Einseitendifferenz, Massenbilanz und Bezugspunktinvarianz.
- Nachweis: docs/P2_VOF_NORMAL_RECONSTRUCTION_PARTIAL.md,
  scripts/p2_vof_normal_reconstruction_acceptance.py,
  ENGINE TEST STUBE/runs/p2_vof_normal_reconstruction_partial.json.
- **P2-P8 bleiben INCOMPLETE, ALL_COMPLETE=False.** Nur lokale
  Gradientenapproximation; keine geometrisch konservative
  Grenzflaechenadvektion, dynamische Zweiwege-FSI, externen
  CFD-Messdaten, globale Energie-/Impulserhaltung oder neues
  Unreal-5.8-Native-E2E.


## P2 Teilfortschritt: geometrischer periodischer PLIC-Volumentransport (31.10.2026)

- Isolierter 3D-Operator p2_plic_periodic_sweep3d mit geometrisch
  integrierter PLIC-Ausfluss-Slabmenge statt diffuser f*Courant-Naeherrung.
  Ein einziger kartesischer, raeumlich konstanter Geschwindigkeitsanteil,
  periodischer Rand und Courant <= 1; andere Randbedingungen explizit
  nicht Teil des Operators. Normalen aus der vorhandenen VOF-Rekonstruktion.
- Unabhaengige analytische 3D-Extrusion des Ebenenschnitts x+y<=1
  und periodische Translation eines PLIC-Wasserblocks in allen drei
  Raumrichtungen und beiden Vorzeichen, jeweils zwei Zeitschritte.
- Native CTest und reproduzierbares Teil-Gate:
  scripts/p2_plic_periodic_sweep_acceptance.py,
  ENGINE TEST STUBE/runs/p2_plic_periodic_sweep_partial.json.
  Details: docs/P2_PLIC_PERIODIC_SWEEP_PARTIAL.md.
- **P2-P8 weiterhin INCOMPLETE; ALL_COMPLETE=False.** Keine unsplit
  3D-MAC-Advektion mit variablen Geschwindigkeiten, keine bewegte
  Zweiwege-FSI, globale physikalische Energie-/Impulsvalidierung,
  externe CFD-Messdaten oder neue Unreal-5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: periodischer PLIC-Schertransport (31.10.2026)

- Geometrischer PLIC-Face-Transport jetzt auch fuer transversale, signwechselnde periodische Scherprofile mit diskret divergenzfreien 1D-Stroemungslinien; eingefrorene Eingangsfelder, atomare Delta-Buchung, CFL- und Divergenzpruefung.
- 8x8x8 und 12x8x6 in allen drei Achsen: 102/102 CTest PASS, 16.397 native Assertions; analytischer Einzelschrittfehler maximal 5.56e-17; zwei Schritte mit festgehaltenen Achsnormalen analytisch, neu rekonstruierte Normalen weichen lokal bis 0.0552724 ab.
- Teilnachweis: docs/P2_PLIC_SHEARED_PERIODIC_PARTIAL.md, scripts/p2_plic_sheared_acceptance.py, ENGINE TEST STUBE/runs/p2_plic_sheared_partial.json. Kein vollstaendiger 3D-MAC-VOF-Transport, keine dynamische FSI oder externe CFD-Validierung.
- **P2 bleibt INCOMPLETE; P3-P8 bleiben INCOMPLETE.**

## P2 Teilfortschritt: dreiachsiger symmetrischer PLIC-Transport (31.10.2026)

- Neue periodische VOF-Normalenrekonstruktion an den Gitter-Nahtstellen und symmetrische Strang-Folge X/2, Y/2, Z, Y/2, X/2 mit jeweils aktualisierten PLIC-Normalen. Drei nichtverschwindende, transversale Schergeschwindigkeitsfelder werden geometrisch konservativ transportiert.
- MSVC Release **103/103 CTest PASS**, **8314 native Assertions**. Unabhaengige analytische 3D-Quadertranslationen (12x10x8 und 20x16x12), zwei dreiachsige Scherfaelle, periodische Naht und Negativtests; Massenresiduum im Rundungsbereich, gemessener relativer L1-Fehler der Quader unter 2 Prozent.
- Nachweis: docs/P2_PLIC_STRANG_PERIODIC_PARTIAL.md, scripts/p2_plic_strang_acceptance.py, ENGINE TEST STUBE/runs/p2_plic_strang_partial.json.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Operator-Splitting und vorgegebene periodische Lanes sind keine unsplit MAC/VOF-Fluidphysik, keine dynamische Zweiwege-FSI, keine globale Energie-/Impulsvalidierung, keine externe 3D-CFD-Messreferenz oder Unreal-5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: unsplit MAC/PLIC mit vorgeschriebenem 3D-Wirbelfeld (31.10.2026)

- Neuer optionaler periodischer 3D-PLIC-Operator mit gleichzeitiger konservativer Face-Delta-Buchung. Alle drei MAC-Komponenten duerfen entlang ihrer eigenen Achse variieren, solange die diskrete 3D-Divergenz verschwindet. Kein Drucksolver oder Navier-Stokes-Impulsupdate.
- MSVC Release 104/104 CTest PASS, 9266 native Assertions; zwei unabhaengige analytische Quader-Translationsreferenzen mit relativen L1-Fehlern 0.0026688889 und 0.0009293333. Zwei echte diskrete 3D-Curl-Felder auf 8^3/12x10x8 mit maximaler Divergenz 1.39e-16 s^-1, zwei konservative VOF-Schritte und analytisch stationaeres Vollwasser.
- Fail-closed bei nichtsolenoidalem Feld, 6-Face-CFL-Ueberschreitung, unaufgeloesten Normalen und Ueberlappung geometrischer Ausfluss-Slabs. Keine erfundene konservative Korrektur.
- Nachweis: docs/P2_PLIC_UNSPLIT_MAC_PARTIAL.md, scripts/p2_plic_unsplit_acceptance.py, ENGINE TEST STUBE/runs/p2_plic_unsplit_partial.json.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Unsplit-Slab-Summe ist noch keine geometrisch exakte mehrdimensionale Raum-Zeit-Advektion; keine Live-MAC/VOF-Integration, dynamische Zweiwege-FSI, unabh. 3D-CFD-Messdaten, globale Energie-/Impulsabnahme oder UE5.8 Native-E2E.

## P2 Teilfortschritt: adaptives periodisches PLIC-CFL-Subcycling (31.10.2026)

- Neuer optionaler 3D-MAC/PLIC-Adapter fuer vorgegebene, waehrend des Zeitschritts eingefrorene diskret divergenzfreie periodische Face-Geschwindigkeiten. Automatische Aufteilung in sichere Sechs-Face-Courant-Teilschritte; neue PLIC-Normalen nach jedem Schritt; explizites Substep-Budget mit Fail-Closed-Verhalten.
- Analytische periodische 3D-Quader- und Randnaht-Translationen, diskret solenoidales variables MAC-Curl-Feld, Bitparitaet zum Einzelschritt-Kernel, Negativtests und Massenbilanz. Die mehrdimensionale Ecke bleibt geometrisch approximiert.
- MSVC Release, vollstaendiger CTest und reproduzierbares Gate: docs/P2_PLIC_ADAPTIVE_SUBCYCLE_PARTIAL.md, scripts/p2_plic_adaptive_acceptance.py, ENGINE TEST STUBE/runs/p2_plic_adaptive_partial.json.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Keine echte Navier-Stokes-Zeitintegration, bewegte Zweiwege-FSI, unabh. gemessene 3D-CFD-Validierung, globale Energie-/Impulsabnahme oder neue UE5.8 Native-E2E-Abnahme.

## P2 Teilfortschritt: zeitlich variierende vorgeschriebene MAC/PLIC-Felder (31.10.2026)

- Neuer Adapter `p2_plic_temporal_schedule3d` transportiert Wasser durch eine Folge zeitlich getrennter 3D-MAC-Face-Geschwindigkeitsfelder. Jedes Intervall hat eigene Dauer und adaptives CFL-Subcycling; zwischen Intervallen werden die Geschwindigkeiten aktualisiert. Keine Mutation der Eingaben, kein stilles Abschneiden des Gesamtbudgets.
- Bitgleiche Referenz gegen die bestehende adaptive Einzelschritt-Implementierung sowie manuell sequenzierte 2-Intervall-Felder; unabhängige analytische periodische 3D-Quader-Netto-Translation und zeitlich variierendes diskret solenoidales MAC-Curl-Feld. Negative Fälle einschließlich zweitem nichtsolenoidalem Intervall und erschöpftem Gesamtbudget.
- Reproduzierbare Native-Tests und quantitative Teilabnahme: `docs/P2_PLIC_TEMPORAL_SCHEDULE_PARTIAL.md`, `scripts/p2_plic_temporal_acceptance.py`, `ENGINE TEST STUBE/runs/p2_plic_temporal_partial.json`.
- **P2–P8 INCOMPLETE; ALL_COMPLETE=False.** Vorgegebene Geschwindigkeiten sind kein Navier-Stokes-Update. Keine bewegte Zweiwege-FSI, unabhängige 3D-CFD-Messvalidierung, globale physikalische Energie-/Impulserhaltung oder neue UE5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: periodische Viskositaet + nichtlinearer MAC/PLIC-Schritt (31.10.2026)

- Neuer optionaler Adapter `p2_viscous_nonlinear_mac_plic3d.hpp`: explizite
  periodische 3D-MAC-Viskositaet (7-Punkt-Stencil, alpha<=1/8, Subcycling),
  anschliessend bestehende nichtlineare Impulsadvektion, Druckprojektion und
  geometrisch konservativer passiver 3D-PLIC-Wassertransport. `nu=0` bleibt
  bitgleich zum bisherigen Schritt; ungueltige Naehte/CFL-Budgets fail-closed.
- Analytische diskrete Fourier-Scher-Eigenmode auf 8^3 und 12^3 mit 1/2
  Viskositaets-Substeps: max. Fehler 1.39e-17; Face-Quadratsumme sinkt,
  periodische Geschwindigkeitskomponentensummen bleiben erhalten.
  Gemischter 12^3-Wasserblock wird tatsaechlich advehiert und die Wasserbilanz
  schliesst. MSVC Release PASS, 106/106 CTest PASS, 11339 native Assertions,
  25/25 quantitative Teilabnahmechecks PASS.
- Nachweis: `docs/P2_VISCOUS_NONLINEAR_MAC_PLIC_PARTIAL.md`,
  `scripts/p2_viscous_nonlinear_mac_plic_acceptance.py`,
  `ENGINE TEST STUBE/runs/p2_viscous_nonlinear_mac_plic_partial.json`.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Konstante Dichte, periodische
  Raender, passive VOF, keine spannungskonsistente freie Oberflaeche,
  bewegte Zweiwege-FSI, gemessene 3D-CFD-Referenzen, globale physikalische
  Energie-/Impulsbilanz oder neue UE5.8-Native-E2E-Abnahme.

## P2 Teilfortschritt: zeitliche viskose MAC/PLIC-Zustandsfortschreibung (31.10.2026)

- Neuer optionaler `p2_viscous_mac_plic_sequence3d`-Adapter: mehrere aufeinanderfolgende periodische 3D-MAC/PLIC-Schritte mit variabler Viskositaet und Zeitschrittdauer, nichtlinearem Impulspredictor, Druckprojektion und passivem geometrischem Wassertransport. Gemeinsame echte PLIC-/Viskositaets-Substep-Budgets und Fail-Closed bei ungueltigen spaeteren Intervallen; Eingangszustand unveraendert.
- Unabhaengige diskrete Fourier-Referenzen fuer drei Intervalle auf 8^3/12^3: maximale Face-Abweichung 7.38e-18; analytische Dampfungsfaktoren 0.8871835194886617/0.94732423526731346. Echter 12^3-Wassertransport und Bitparitaet mit manuell verketteten Einzelschritten; 9 Negativfaelle.
- MSVC C++17-Release Native-Test 11.364 Assertions PASS; bestehende 106/106 CTest PASS; separates Python-Teil-Gate 21/21 PASS. Nachweis: `docs/P2_VISCOUS_MAC_PLIC_SEQUENCE_PARTIAL.md`, `scripts/p2_viscous_mac_plic_sequence_acceptance.py`, `ENGINE TEST STUBE/runs/p2_viscous_mac_plic_sequence_partial.json` mit SHA256-Fingerprints.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Keine stresskonsistente Zweiphasenphysik, bewegte Zweiwege-FSI, externen 3D-CFD-Messdaten, globale physikalische Energie-/Impulsabnahme oder neue Unreal-5.8-Native-E2E-Abnahme. Die neuen Tests sind ein separater Native-Build und noch kein CTest-Target.

## P2 Teilfortschritt: Sequenzweites Wasser-Masseninventar (31.10.2026)

- Der optionale periodische viskose MAC/PLIC-Sequenzadapter protokolliert jetzt Wasser-Start-/Endvolumen (m3), Wassermasse bei konstanter Dichte (kg), Intervall-Grenzspruenge, Teleskop-Bilanz, maximale projizierte Divergenz und aggregierte viskose Face-Summen-Proxies. Diskontinuitaeten und Bilanzfehler werden explizit abgewiesen.
- Unabhaengige 8^3-/12^3-Fourier-Eigenmoden und 12^3-Wasser-Slab: 0.43200000000000011 m3 / 432.00000000000011 kg, Intervall-Sprung 0, Teleskop-Residuum 0, max. Divergenz 0 im Wasser-Slab. Native 11391 Assertions, MSVC Release, 106/106 CTest, Python-Teilabnahme 28/28 PASS.
- Belege: docs/P2_VISCOUS_MAC_PLIC_SEQUENCE_PARTIAL.md, scripts/p2_viscous_mac_plic_sequence_acceptance.py und ENGINE TEST STUBE/runs/p2_viscous_mac_plic_sequence_partial.json (Quell-/Binaer-/Log-SHA256).
- **P2-P8 bleiben INCOMPLETE, ALL_COMPLETE=False.** Diese numerische Inventar-Bilanz beweist weder physikalische Impuls-/Energieerhaltung noch dynamische Zweiwege-FSI, spannungskonsistente freie Oberflaechen, unabhaengige 3D-CFD-Messvalidierung oder UE5.8 Native-E2E dieses Pfades.

## P2 Teilfortschritt: periodische MAC-Bulk-Impuls-/Energieinventur (01.11.2026)

- Der optionale mehrschrittige 3D-MAC/PLIC-Adapter berechnet nun pro
  Intervall und sequenzweit SI-skalierte konstante-Dichte-Bulk-Impulse
  (kg m/s) und kinetische Energie (J) auf eindeutigen periodischen
  MAC-Faces. Teleskop-Residuen pruefen die Buchungskonsistenz.
- Unabhaengige diskrete Fourier-Scherreferenz auf 8^3 und 12^3:
  Anfangsenergien 0.3072 J / 1.0368 J, Endenergien
  0.2635969735172683 J / 0.9658989204881785 J;
  maximale absolute Energiedifferenzen 5.00e-16 / 1.25e-14 J.
  Periodische Mittelimpulse 10.24 / 34.56 kg m/s. 3D-Wasserblock
  432 kg bei unveraenderter Masse. Acht Negativtests.
- Separater MSVC-Native-Test 100 Assertions PASS; vollstaendige
  bestehende 106/106 CTest-Suites PASS; 23/23 Python-Gate-Checks.
  Nachweis: docs/P2_MAC_BULK_BUDGET_PARTIAL.md,
  scripts/p2_mac_bulk_budget_build.cmd,
  scripts/p2_mac_bulk_budget_acceptance.py,
  ENGINE TEST STUBE/runs/p2_mac_bulk_budget_partial.json.
- **P2-P8 weiterhin INCOMPLETE; ALL_COMPLETE=False.** Die Quadratur
  ist konstante-Dichte-Bulk-Diagnostik ueber alle Zellen, keine
  wassergewichtete Zweiphasen-Gesamtenergie oder vollstaendige
  physikalische Impuls-/Energieerhaltung. Dynamische FSI, freie
  Oberflaechen, unabh. CFD-Messvalidierung und UE5.8 Native-E2E
  bleiben offen.

## P2 Teilfortschritt: wassergewichtete MAC/VOF-Impuls-/Energiequadratur (01.11.2026)

- Optionaler periodischer 3D-MAC-Diagnoseoperator gewichtet Zellmittel-Geschwindigkeiten mit dem realen VOF-Wasseranteil; SI-Wassermasse, Wasserimpuls und Wasser-Kinetik statt bisheriger Vollraum-Bulk-Quadratur. Der Endpunktadapter vergleicht den echten viskosen MAC/PLIC-Sequenzzustand und die vorhandene Wasserinventur.
- Unabhaengige 8^3/12^3-Referenzen fuer konstante 3D-Vektorgeschwindigkeit, teilgefuellte sinusfoermige Scherung und drei nichtverschwindende 3D-Curl-Komponenten mit fraktionalem Wasseranteil; 12 Negativtests. Reale dreischrittige Wasserfortschreibung: 432 kg Start/Ende, Energiequadratur 0.25919999999999971 J -> 0.25680365934298033 J (keine physikalische Energieerhaltungsaussage).
- MSVC-C++17-Release-Nativtest 40 Assertions PASS, komplette 106/106 CTest PASS, 40/40 Python-Teil-Gates PASS. Belege: docs/P2_WATER_MAC_INVENTORY_PARTIAL.md, scripts/p2_water_mac_inventory_acceptance.py, ENGINE TEST STUBE/runs/p2_water_mac_inventory_partial.json mit SHA256.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Zellmittelung/VOF-Gewichtung ist keine subzellgenaue Zweiphasen-Impulsadvektion, keine dynamische FSI oder physikalische Gesamtenergie-/Impulserhaltung. Unabhaengige gemessene 3D-CFD-Referenzen und Unreal-5.8-Native-E2E dieses Pfades fehlen.

## P2 Teilfortschritt: exakte dreidimensionale ABC-Navier-Stokes-Referenz (01.11.2026)

- Neuer unabhaengiger **kontinuierlicher** analytischer 3D-Navier-Stokes-Test fuer das periodische ABC-Beltrami-Feld (alle drei Komponenten und Achsen, nichtlineare Impulsadvektion, Viskositaet, Druckprojektion, Wasser-Slab-PLIC). Physikalische Daempfung exp(-nu*k^2*t), Druck absorbiert den nichtlinearen Gradienten.
- Ein Zeitschritt von 0.02 s, Gitter 8^3/12^3/16^3: relative L2-Fehler 0.00177056535 / 0.00126424513 / 0.00097379950, max. projizierte Divergenz 6.82e-15 s^-1, Wasserbilanzresiduum < 1.4e-16 m3. Verfeinerung verbessert den Fehler um Faktor 1.40049 und 1.29826. Numerische Energie zeigt zusaetzliche Upwind-Dissipation gegenueber exakter viskoser Referenz.
- MSVC Release und 106/106 bestehende CTests PASS, 25366 native Assertions und 32/32 quantitative Teilchecks. Nachweise: docs/P2_ABC3D_ANALYTIC_PARTIAL.md, scripts/p2_abc3d_analytic_acceptance.py, ENGINE TEST STUBE/runs/p2_abc3d_analytic_partial.json.
- **Bekannte echte Blockade:** 5-Schritt-ABC/VOF-Test auf 12^3 scheitert mit "Unresolved periodic PLIC interface"; Mehrschrittfall ist NICHT abgenommen. Kein erfundener Fallback.
- **P2-P8 bleiben INCOMPLETE; ALL_COMPLETE=False.** Analytische 3D-Referenz ist keine gemessene externe CFD-Validierung. Stresskonsistente freie Oberflaechen, dynamische Zweiwege-FSI, globale Energie-/Impulsabnahme und UE5.8 Native-E2E dieses Pfades fehlen.

## P2 Teilfortschritt: ABC3D-Mehrschritt und PLIC-Endpunkt-Rundung (01.11.2026)

- Die bisherige 5-Schritt-Blockade wurde reproduziert: nach Schritt 1 gab es 19 Zellen mit exakt `nextafter(1.0,0.0)`, davon vier mit verschwindendem zentralem PLIC-Gradienten. Der unveränderte Standardpfad scheitert korrekt in Schritt 2.
- Neuer **opt-in** Mehrschritt-Adapter bereinigt ausschließlich Füllgrade innerhalb acht Maschinen-Epsilons von 0/1. Kumulierte absolute Korrektur wird begrenzt und bilanziert. Echte unbestimmbare Grenzflächen und ungültige Eingaben bleiben abgelehnt.
- 12³-ABC-Feld, 5 × 0.02 s: vollständige Viskositäts-/Impuls-/Druck-/PLIC-Fortschreibung, relativer L2-Fehler 0.00624613626, Wasser 0.25 m³, absolute Rundungsänderung 5.67e-18 m³, Gesamtresiduum -1.72e-16 m³, Divergenz 1.16e-14 s^-1. Zusätzliche numerische Energiedissipation gegenüber analytischer Lösung bleibt.
- Separater Native-Test, CTest und Hash-Gate: `docs/P2_ABC3D_MULTISTEP_ROUNDOFF_PARTIAL.md`, `scripts/p2_abc3d_multistep_probe_build.cmd`, `scripts/p2_abc3d_multistep_roundoff_acceptance.py`, `ENGINE TEST STUBE/runs/p2_abc3d_multistep_roundoff_partial.json`.
- **P2–P8 INCOMPLETE; ALL_COMPLETE=False.** Kein vollständiger 3D-Zweiphasen-Stresssolver, keine dynamische Zweiwege-FSI, keine externen CFD-Messdaten, keine globale physikalische Energie-/Impulsabnahme oder Unreal-5.8-Native-E2E dieses Pfades.

## P2 Teilfortschritt: zeitvariable 3D-ABC-MAC/PLIC-Sequenz (01.11.2026)

- Neuer opt-in-Schedule-Adapter mit variablen dt/nu, per-Intervall-Wasser-/Rundungsledger und kumulativen Substep-Budgets. Nur Rundungsendpunkte innerhalb acht Maschinen-Epsilons werden bereinigt, echte unaufloesbare PLIC-Normalen weiterhin abgelehnt.
- 12^3-ABC-Navier-Stokes-Referenz ueber 5 variable Intervalle (0.1 s): relativer L2-Fehler 0.00623480561, max. Geschwindigkeitsabweichung 0.00051374214 m/s, numerisches/analytisches Energieratio 0.96327611/0.97512732, Wasser 0.25000000000000022 m3, Massensaldo -5.43e-18 m3, absolute Snapkorrektur 6.65e-18 m3, max. Divergenz 7.23e-15 s^-1. Einzelintervall-Komposition und alte konstante Sequenz sind numerisch exakt gleich (max_diff=0).
- MSVC-C++17-Native-Test 6955 Assertions PASS, 107/107 CTest PASS (neuer Regressionstest in CMake registriert); reproduzierbares Gate: 'docs/P2_ABC3D_VARIABLE_SCHEDULE_PARTIAL.md', 'scripts/p2_abc3d_variable_schedule_build.cmd', 'scripts/p2_abc3d_variable_schedule_acceptance.py', 'ENGINE TEST STUBE/runs/p2_abc3d_variable_schedule_partial.json'.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Weiterhin kein stresskonsistentes 3D-Zweiphasen-CFD, keine Zweiwege-FSI, keine gemessenen externen 3D-CFD-Referenzen, keine globale physikalische Energie-/Impulserhaltung und kein UE5.8 Native-E2E dieses Adapters.

## P2 Teilfortschritt: 3D-ABC-Zeitverfeinerung bei variabler Viskositaet (01.11.2026)

- Unabhaengige kontinuierliche 3D-ABC-Navier-Stokes-Referenz fuer 5/10/20 gekoppelte MAC/PLIC-Zeitschritte mit identischem Endzeitpunkt 0.1 s, identischem 12^3-Gitter und identischem Integral der variablen Viskositaet. Echte nichtlineare Impulsadvektion, Druckprojektion, Viskositaet und Wasser-PLIC in jedem Schritt.
- Analytischer relativer L2-Fehler sinkt 0.00623480561 -> 0.00622325331 -> 0.00621749743. Numerische Selbstkonvergenz der Geschwindigkeiten: paarweise Differenzen 5.04915e-5 / 2.49935e-5, Quotient 2.02018 (zeitlich ungefaehr erste Ordnung). Numerische kinetische Energieratio bei 20 Schritten 0.96330574 gegenueber analytisch 0.97512732: verbleibende raeumliche/numerische Dissipation ist explizit dokumentiert.
- Wasservolumen 0.25000000000000022 m3, Ledger -2.04e-18 m3, max. Divergenz 3.01e-15 s^-1. Dauerhaftes CMake-CTest-Target, MSVC Release, separate quantitative SHA256-Teilabnahme: docs/P2_ABC3D_TEMPORAL_CONVERGENCE_PARTIAL.md, scripts/p2_abc3d_temporal_build.cmd, scripts/p2_abc3d_temporal_acceptance.py, ENGINE TEST STUBE/runs/p2_abc3d_temporal_partial.json.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Zeitkonvergenz auf einem festen Gitter ist keine unabhaengige gemessene 3D-CFD-Validierung, keine spannungskonsistente freie Oberflaeche, keine dynamische Zweiwege-FSI, keine globale physikalische Energie-/Impulserhaltung und kein UE5.8 Native-E2E.


## P2 Teilfortschritt: Galilei-verschobene exakte 3D-ABC-Referenz (01.11.2026)

- Neue echte Navier-Stokes-Referenz mit konstanter mittlerer 3D-Translation
  U=(0.25,-0.15,0.10) m/s. Damit wird nichtlineare Advektion auch dann
  geprüft, wenn die stationäre ABC-Beltrami-Lösung allein zu schwach wäre.
- MSVC Release, 109/109 CTest, 12.987 native Assertions: relative L2-Fehler
  auf 8³/10³/12³ = 0.00847434/0.00678842/0.00564164. Die bewegte
  Referenz ist deutlich näher als die unverschobene; Mittelimpulse,
  passive Wasserbilanz (0.5 m³), Druckprojektion und numerische
  Viskositätsdämpfung sind quantitativ geprüft.
- Ein explorativer 16³-Lauf scheiterte mit `Unresolved periodic PLIC
  interface`, auch mit voll gefülltem VOF. **Kein 16³-PASS**;
  PLIC-interne Zwischenstufen/Rundung bleiben eine offene Blockade.
- Beleg: `docs/P2_ABC3D_GALILEAN_PARTIAL.md`,
  `scripts/p2_abc3d_galilean_acceptance.py` und
  `ENGINE TEST STUBE/runs/p2_abc3d_galilean_partial.json`.
- **P2 bleibt PARTIAL, P3–P8 INCOMPLETE.** Kein vollständiges
  Zweiphasen-CFD, dynamische Zweiwege-FSI, externes CFD-Messbenchmark,
  globale physikalische Erhaltung oder neues UE5.8-Native-E2E.

## P2 Teilfortschritt: exakte 16^3-Reinphasen-MAC/PLIC-Invarianz (01.11.2026)

- Numerisch konservative Sonderbehandlung **nur** fuer exakt uniforme
  VOF=0 oder VOF=1 nach erfolgreicher MAC-Divergenz-/CFL-Pruefung.
  Verhindert kuenstliche PLIC-Teilzellen ohne aufloesbare Normale bei
  vollstaendig gefuelltem 16^3-ABC-Flow. Gemischte unaufloesbare
  Grenzflaechen bleiben fail-closed; kein allgemeines Normalen-Fallback.
- Zwei reale gekoppelte 16^3-ABC-Simulationsschritte pro reiner Phase,
  Viskositaet, nichtlineare Impulsadvektion, Projektion und PLIC:
  Volumen exakt 1/0 m3, null Snap, null Bilanzresiduum, 4 PLIC- und
  2 viskose Substeps, max. Divergenz 1.11e-14 s^-1.
- MSVC Release, 37 Assertions, 5 Negativfaelle, 110/110 CTest PASS.
  Nachweise: docs/P2_PLIC_PURE_PHASE16_PARTIAL.md,
  scripts/p2_plic_pure_phase16_acceptance.py,
  ENGINE TEST STUBE/runs/p2_plic_pure_phase16_partial.json.
- **P2-P8 INCOMPLETE; ALL_COMPLETE=False.** Der 16^3-Slab-Fall mit
  tatsaechlicher Wasser-Luft-Grenzflaeche bleibt offen. Keine dynamische
  FSI, stresskonsistente Zweiphasenphysik, externen CFD-Messdaten,
  globale Energie-/Impulsvalidierung oder neue UE5.8-Native-E2E.

## P2 Teilfortschritt: PLIC-Endpunktkorrektur je Unterzeitschritt (01.11.2026)

- Realer 16^3 Galilei-ABC/Viscosity/MAC/PLIC-Fehler isoliert: von zwei PLIC-Teilschritten scheitert der zweite; nach dem ersten: 515 Mischzellen, davon drei nur im 0/1-Rundungsbereich unaufloesbar.
- Opt-in `p2_plic_endpoint_subcycle3d` bereinigt nur Werte innerhalb von acht Maschinen-Epsilon nach jedem PLIC-Teilzeitschritt, mit explizitem absolutem Korrekturvolumenbudget und unveraendertem Legacy-Default.
- 16^3-Referenz bestand mit 7 Korrekturen und 1.8973538018496328e-19 m^3 absolutem Korrekturvolumen, im realen viskosen MAC/PLIC-Schritt nachgewiesen. Echte geometrische Mehrdeutigkeit bleibt abgelehnt.
- Dokumentation: `docs/P2_PLIC_SUBSTEP_ENDPOINT_PARTIAL.md`; Nativtest und Reproduktionsskripte: `p2_plic_substep_fix_*`. Vollphase P2 bleibt PARTIAL; P3-P8 weiterhin unvollstaendig.


## P2 Teilfortschritt: echte externe MARIN/SPHERIC-Test-02-Messdaten gegen 3D-UPC (01.11.2026)

- Offizielle SPHERIC-Test-02-Quelle, archiviertes ZIP und Original-XLS mit SHA-256 verifiziert; 7.395 Messzeilen mit 8 Druck- und 4 Höhenkanälen geprüft. Originaldateien und lokale Python-Abhängigkeit bleiben unter `references/` und sind aus Git ausgeschlossen.
- Win64/MSVC Release: echte 3D-`CoupledMacWater3D`-Dammbruchsimulation mit Hindernis, 32×10×10 Zellen, 200 Schritten bis 2,00 s. Die Geometrie wurde gegen das offizielle PDF und eine unabhängige Referenz korrigiert (H4=2,632 m, Hindernis x=0,6635–0,8245 m). Numerische Wasservolumendrift 1,89×10⁻¹⁵ m³.
- Gegen die **echten** Laborhöhen: RMSE H1/H2/H3/H4 = **0,090425 / 0,100170 / 0,107548 / 0,206725 m**. Alle vier verletzen das vorab definierte 0,05-m-Limit. **Physikalische Validierung: FAIL.**
- 9/9 positive und manipulationsbasierte Regressionstests PASS; das ist nur eine Integritäts-/Ausführungs-Teilabnahme, **kein P2-Vollphasen-PASS**. Fehlend: P1–P8-übergreifende vollständige E2E-Physik, 6-s-Lauf, Drucksensoren, Netzkonvergenz, echte Luft-Wasser-Spannungen und dynamische Zweiwege-FSI.
- Reproduktion: `scripts/p2_spheric_marin_partial_acceptance.py`; Report: `ENGINE TEST STUBE/runs/p2_spheric_marin_partial_acceptance.json`; Details: `docs/P2_SPHERIC_MARIN_EXTERNAL_BENCHMARK_PARTIAL.md`.
- **P2 bleibt PARTIAL, P3–P8 INCOMPLETE, ALL_COMPLETE=False.**


## P2 Teilfortschritt: gesaettigter 3D-VOF-Durchfluss vs. echte MARIN-Daten (01.11.2026)

- Im bisherigen konservativen Wassertransport der Live-3D-MAC-API blockierte die Vorkapazitaet einer vollstaendig gefuellten Zelle unphysikalisch den gleichzeitigen Ein- und Ausfluss. Ein explizit zuschaltbarer 3D-Donor-Cell/VOF-Durchfluss mit simultanen Face-Fluessen und konservativer Zellkapazitaetspruefung wurde umgesetzt; Standardpfad bleibt unveraendert.
- Unabhaengiger analytischer 16x8x8-Slab-Translationstest: hinterer Fuellgrad 0,9, neue Front 0,1, 384 interne Transfers, erhaltene Wassermasse. Native Regression 2.448 Assertions PASS, kompletter MSVC Release/CTest 113/113 PASS.
- Original MARIN/SPHERIC Test 02: echter Win64-3D-Solverlauf ueber 200 Schritte mit identischer Geometrie und gemessenen Hoehen H1-H4. Vergleich gegen vorherigen Legacy-Lauf: RMSE H1 0,090425 -> 0,089234 m, H2 0,100170 -> 0,079381 m, H3 0,107548 -> 0,092056 m, H4 0,206725 -> 0,157912 m. Alle vier verbessert, **alle vier ueber dem unabhaengig gesetzten 0,05-m-Grenzwert**. Keine externe physikalische Abnahme.
- Teilnachweis: docs/P2_SPHERIC_MARIN_THROUGHFLOW_PARTIAL.md und scripts/p2_vof_throughflow_marin_acceptance.py sowie gehashte Native-/CTest-/Vergleichsbelege im bestehenden ENGINE TEST STUBE/runs.
- **P2 weiterhin INCOMPLETE**: echte Stressrandbedingungen an der Wasser/Luft-Grenze, dynamische Zweiwege-FSI, gemessene 3D-CFD-Referenz innerhalb Toleranz, globaler physikalischer Energie-/Impulshaushalt fehlen. P3-P8 ebenso Vollphasen-offen. ALL_COMPLETE=False.


## P2 Teilfortschritt: unabhängige MARIN-Messwerte mit zwei echten 3D-Gittern und vollständigen Zeitreihen (01.11.2026)

- Nach lokalem MSVC-Neubau wurden **zwei vollständige native 3D-Simulationen** auf 32x10x10 (dx=0,10m) und 64x20x20 (dx=0,05m) gerechnet, jeweils 200 Schritte zu je 0,01 s, identische physische Tankgeometrie (3,20 x 1,00 x 1,00m), reale Messdaten von SPHERIC Test 02 als externe Referenz.
- Anstelle von bisher nur wenigen selektiven Stichproben sind jetzt **alle 200 berechneten Vergleichszeitpunkte plus Anfangszustand** gespeichert, mit manipulationsgeschützter Auswertung, getrenntem Massenerhaltungsprotokoll und acht positiven/negativen Regressionstests. Vollständige 113/113 bestehende CTest-Suites bestanden.
- Höhe RMSE (grobes => feines Gitter), jeweils über die gesamte simulierte 2-s-Zeitreihe: H1 **0,120670 => 0,103996 m**, H2 **0,116936 => 0,076746 m**, H3 **0,162034 => 0,139819 m**, H4 **0,205611 => 0,158084 m**. **Alle vier verbessern sich, aber alle vier verfehlen das vorab festgelegte 0,05-m-Ziel.** Eine frühere Messung mit H2=0,044693 m bezog sich nur auf selektive Zeitpunkte und war daher nicht zur Vollvalidierung geeignet.
- Beide 3D-Läufe halten Wasser-Volumendrift unter **3e-12 m³**. Der Fein-Gitterlauf beansprucht deutlich längere native Rechenzeit; kein künstlicher PASS durch Abbruch, Auslassen oder interpolierte Fake-Messungen.
- Selbst das Bestehen dieses neuen zweigitterigen Referenzvergleichs der **Ausführung** (nicht der Physik) beweist keine asymptotische Gitterkonvergenz. Fehlend: experimentelles 6-s-Fenster, P1–P8-Drucksensoren, stresskonsistente Wasser/Luft-Grenze, dynamische Zweiwege-FSI und globale physikalische Energiebilanz. **P2 bleibt PARTIAL; keine Vollabnahme.**
- Nachweis und Wiederholung: docs/P2_MARIN_GRID_REFINEMENT_PARTIAL.md, scripts/p2_spheric_marin_grid_partial_acceptance.py --rebuild-and-rerun, ENGINE TEST STUBE/runs/p2_marin_grid_refinement_partial_acceptance.json. ALL_COMPLETE=False.


## P2 Teilfortschritt: acht native Drucksensor-Proxys gegen reale SPHERIC/MARIN-Daten (01.11.2026)

- Der bisher fehlende Drucksensor-Auslesepfad für den offiziellen MARIN/SPHERIC-3D-Dammbruchtest wurde als read-only MAC-Druck-Gauge-Sampling implementiert; echte Boxfront-P1–P4- und Dach-P5–P8-Lagen nach Testfall-PDF Abbildung 7. Jeder Zeitschritt liest das **tatsächliche** Fluiddruckfeld, keine hydrostatischen Fake-Werte. Die Labor-Referenz ist das gesicherte originale XLS mit 7.395 Datenzeilen.
- In zwei realen MSVC-Win64-3D-Läufen mit 32x10x10 (dx=0,10 m) und 64x20x20 (dx=0,05 m) wurden pro Gitter acht numerische Druckzeitreihen bei 201 Zuständen erzeugt. Im gröberen Gitter haben P1/P2 und P6/P7 unvermeidliche Zell-Aliase, im feineren P3/P4; die Messgeometrie ist **nicht voll aufgelöst**.
- Unabhängiger RMSE über 200 echte Simulationszeitpunkte (Pa), grob => fein: P1 **5646 => 18178**, P2 **6025 => 18691**, P3 **5053 => 20274**, P4 **5274 => 20342**, P5 **5254 => 26373**, P6 **5345 => 16060**, P7 **5317 => 16108**, P8 **5736 => 16356**. **Alle acht Druck-RMSE verschlechtern sich beim verfeinerten Gitter.** Die zuvor nachgewiesene Verbesserung der Wasserstands-Höhen ist kein Beleg für richtige Wanddrücke und Impulse. Es fehlen Stressrandbedingungen, dynamische Zweiwege-FSI und globaler Energie-/Impulsabschluss.
- Native CMake/MSVC Release und **114/114 CTest-Suites bestanden**, zusätzlich 184 native Mapping-Assertions und 11 Python-Integritäts-/Negativtests. Die komplette Prüfung wurde nach einem Rebuild und echten erneuten 200-Step-Läufen ausgeführt. Das bestätigt nur Ausführung/Datenintegrität, **nicht** physikalische Validierung.
- Quell- und Nachweisübersicht: docs/P2_MARIN_3D_PRESSURE_SENSORS_PARTIAL.md, scripts/p2_marin_pressure_build.cmd, scripts/p2_marin_pressure_partial_acceptance.py und ENGINE TEST STUBE/runs/p2_marin_pressure_partial_acceptance.json.
- **P2 bleibt PARTIAL; keine Vollphasen-Abnahme.** An erster Stelle stehen Druckprojektion und physikalisch konsistente Wandintegration mit aufgelösten Druckmessstellen, bevor eine vollständige P2-Abnahme erwogen werden kann. ALL_COMPLETE=False.


## P2 Teilfortschritt: Wandtangentiale P1–P8-MAC-Druckinterpolation, experimentell weiter FAIL (01.11.2026)

- Nächstzell-Druckproxy an der MARIN/SPHERIC-Box wurde durch **optionale bilineare Wandtangential-Interpolation** mit Wasserfüllgradgewichtung, Festkörper-/Trockenzellen-Ausschluss und deterministischer Stencil-Prüfung ergänzt. Originale Druckprojektion und Legacy-Auswertung unverändert. Manufactured-affine-Felder werden exakt reproduziert; quadratischer tangentialer Fehler sinkt mit halber Zellweite. **Keine Wandnormal-/Stressrekonstruktion, kein physikalisch aufgelöstes Transducer-Footprint.**
- Zwei tatsächlich erneut gerechnete MSVC-Win64-3D-Dammbruchläufe (dx=0,10 m und 0,05 m; je 200 Schritte, 201 Zeilen) gegen die gepinnte Original-MARIN-Druckmessung: interpolierter RMSE in Pa, grob => fein: P1 **5642 => 18166**, P2 **5953 => 18523**, P3 **5553 => 19045**, P4 **5381 => 20883**, P5 **5271 => 16203**, P6 **5316 => 16062**, P7 **5299 => 16120**, P8 **5481 => 16312**. Gegenüber der historischen Nächstzell-Auswertung verbessern sich 5/8 Kanäle pro Gitter; **alle 8/8 verschlechtern sich bei Gitterverfeinerung**. Die originalen H1–H4-Höhen-RMSE liegen auf beiden Gittern ebenfalls jeweils über 0,05 m. Die Druckdiagnose verändert die vollständigen 201-Zeitpunkt-Höhen-/Volumenzeitreihen gegenüber den historischen Native-Läufen um exakt 0. Damit ist die echte Druckkonvergenz weiterhin nachweislich nicht erreicht.
- Unabhängige Native-Tests: **175 Assertions**, **15/15** positive und gezielt korrumpierte Python-Datenintegritätstests, gesamter MSVC-Release/CTest **115/115 PASS**. Nur Teilnachweis für Ausführung und tangentiale Interpolationsmathematik, kein physikalischer P2-PASS.
- Vollständig gehashte Native-/Labor- und Negativtestbelege: docs/P2_MARIN_WALL_TANGENT_PRESSURE_INTERP_PARTIAL.md, scripts/p2_marin_wall_interp_partial_acceptance.py, ENGINE TEST STUBE/runs/p2_marin_wall_interp_partial_acceptance.json. Historische Nächstzell-Native-Belege sind separat archiviert und werden nicht als aktuelle Neubuild-Abnahme umgedeutet.
- **P2 bleibt PARTIAL, ALL_COMPLETE=False.** Nächste offene Kernarbeit: druck- und spannungskonsistente Wasser-Luft-/Wandgrenzbedingungen mit geometrisch aufgelösten Transducern, experimentelles >=6-s-Fenster, dynamische Zweiwege-FSI und globale Energie-/Impulsbilanz.


## P2 Teilfortschritt: echte transaktionale 3D-MAC-PCG-Druckprojektion (01.11.2026)

- Explizit zuschaltbarer Jacobi-PCG-Löser für die vorhandene 3D-MAC-Poisson-Gleichung mit Ghost-Fluid-Wasser/Luft-Druckrand, Festkörper-/Außenwand-Neumannbedingungen, Mittelwert-Eichung getrennter geschlossener Wasserkavitäten und echter Nachprüfung von Matrixrest und Nasszell-Divergenz.
- In den gekoppelten 3D-MAC/VOF-Live-Solver integriert, einschließlich Post-Transport-Reprojektion, atomarem Rollback und auslesbarer Iterations-/Restdiagnose. Original-Gauss-Seidel-Standardpfad bleibt erhalten.
- Echte MSVC-Win64-Release-Tests: 621 + 1.462 native Assertions PASS, analytische 8³/12³/16³-Hydrostatik, Druckvergleich mit 7.500 Gauss-Seidel-Sweeps, echter 3D-Dammbruch mit Massenbilanz, negative Unterkonvergenz-/NaN-/Parameter-/Rollback-Prüfungen; gesamte CTest-Suite **117/117 PASS**.
- Gehashte Quellen, Testbinaries und CTest-Log: `docs/P2_MAC_PCG_PROJECTION_PARTIAL.md`, `scripts/p2_pcg_partial_acceptance.py`, `ENGINE TEST STUBE/runs/p2_pcg_partial_acceptance.json`.
- **P2 bleibt INCOMPLETE:** neuer Löser ist ein numerischer Teilfortschritt und kein Nachweis experimenteller MARIN-Druckkonvergenz oder physikalisch stresskonsistenter Zweiphasen-/FSI-Grenzbedingungen. Das 6-s-Fenster, Sensor-Footprints und globale Impuls-/Energieabnahme bleiben offen; `ALL_COMPLETE=False`.


## P2 Teilfortschritt: numerisch konvergierte 3D-MAC-Druckprojektion gegen reale SPHERIC/MARIN-Daten (01.11.2026)

- Neuer opt-in Jacobi-PCG-Drucklöser für dieselbe diskrete 3D-MAC-Poisson-Gleichung mit nasser Wasser/Luft-Dirichlet-Geisterzellbedingung und geschlossenem Neumann-Druckbezug; physikalische Wasser-/Festkörpergrenzen bleiben als Modellannahmen ausgewiesen. Der bisherige Gauss-Seidel-Standard wurde nicht ersetzt.
- Reale Native-MSVC-3D-Regression: analytischer hydrostatischer Gleichgewichtsdruck mit 8³/12³/16³-Gittern, gemischte 3D-Solid-/Free-Surface-Szenen, unabhängiger 7500-Sweep-GS-Vergleich, negativer Underconvergence-/NaN-/Rollback-Test und echte gekoppelte 3D-Wassertransportintegration. Der separat regulär ausgeführte MAC-PCG-Partial-Gate enthält 117 CTest-Suites.
- Echte 200-Zeitschritt-Neuberechnungen auf 32×10×10 und 64×20×20 mit unverändertem SPHERIC/MARIN-Experiment und gleichem gemessenen P1–P8-Druckfeld und H1–H4-Wasserstands-Sensoren. Beim groben Gitter sinken die P1–P8-RMSE-Werte gegenüber Gauss-Seidel von ca. 5,3–6,0 kPa auf 4,15–4,87 kPa; beim feinen Gitter von 16,1–20,9 kPa auf 3,06–3,69 kPa. Alle acht Druck-RMSE verbessern sich mit Gitterverfeinerung; zuvor verschlechterten sich alle acht.
- Nasse maximale 3D-Projektionsdivergenz grob 5,71e-9 s⁻¹, fein 1,41e-8 s⁻¹. Wasservolumendrift kleiner 1e-13 m³. Die vier Höhen-RMSE bleiben am feinen Gitter bei 0,1039 / 0,0767 / 0,1400 / 0,1591 m und verletzen alle das 0,05-m-Limit.
- Native-/Mess-Nachweise: docs/P2_MARIN_PCG_MEASURED_PARTIAL.md, scripts/p2_marin_pcg_compare.py --fine, tests/p2_marin_pcg_experimental_regression.py, scripts/p2_marin_pcg_measured_partial_acceptance.py und ENGINE TEST STUBE/runs/p2_marin_pcg_measured_partial_acceptance.json. Die Quell- und Runtime-SHA256 werden separat geprüft.
- P2 bleibt INCOMPLETE. Ein numerisch konvergenter diskreter 3D-Drucklöser validiert noch keine spannungsrichtige bewegliche Wasser/Luft-Grenze, experimentell korrekte Kräfte/Transducerdrücke, 6-s-Gesamtsimulation, dynamische Zweiwege-FSI oder globalen Energie-/Impulsabschluss. ALL_COMPLETE=False.

## P2 Teilfortschritt: volles 6-s-MARIN-Fenster, Native-3D-MAC-PCG (01.11.2026)

- Ein echter 600-Step-MSVC-Win64-Lauf auf 32x10x10 mit dt=0,01 s, PCG-Projektion, konservativem 3D-VOF-Throughflow und P1-P8-Druckproxys wurde von t=0 bis t=6,00 s vollstaendig durchgerechnet. 601 Zustaende, 12 Sensoren gegen die originalen 7.395 SPHERIC/MARIN-Messzeilen.
- Wasser-Volumendrift 2,13e-14 m3, maximale nasse MAC-Divergenz 5,71e-9 s^-1. Die echte Messvalidierung **FAIL**: H1/H2/H3/H4 RMSE = 0,136525/0,091500/0,148245/0,224399 m, alle ueber dem vorab gesetzten 0,05-m-Limit. P1-P8-Druck-RMSE 3,25-4,09 kPa ohne aufgeloeste Transducer-Stressreferenz.
- Native MSVC-Rebuild, 117/117 CTest PASS, sechs unabhaengige Python-Regressionstests mit sechs gezielten Korruptionsnegativfaellen und nativer 601-Step-Ablehnung PASS. Reale Trace-, Binary-, Quellen- und Testlog-SHA256 in `ENGINE TEST STUBE/runs/p2_marin_pcg_6s_partial.json`; Detailnachweis `docs/P2_MARIN_PCG_SIX_SECOND_PARTIAL.md`.
- **P2 bleibt INCOMPLETE**, ebenso P3-P8. Die volle 6-s-Zeitreihe existiert bisher nur auf dem groben Gitter. Unabhaengige feine 6-s- und dritte Konvergenzgitter, geometrisch aufgeloeste Wanddrucksensoren, stresskonsistente Wasser-Luft-Grenze, dynamische Zweiwege-FSI und globale Impuls-/Energiebilanz fehlen. `ALL_COMPLETE=False`.

## P2 Teilfortschritt: bilineare H1-H4-VOF-Spaltenmessung (01.11.2026)

- Optionaler subzellulärer x/y-Höhenprobe-Operator in echter 3D-MAC/VOF-Simulation, mit analytischen 32x10x10-/64x20x20-Tests, NaN-/Randprüfungen und 60 nativen Assertions. Druckprojektion, VOF-Physik und Legacy-Auslesung bleiben unverändert.
- Ein neuer echter 200-Step-MSVC-PCG-Lauf auf 32x10x10 bis 2,00 s: Vergleich mit vorhandener nativer Kontrollspur und originalen MARIN/SPHERIC-Messwerten. 12 Nicht-Höhen-Signale (inklusive P1-P8, Volumen, Divergenz) an allen 201 Zeitpunkten exakt identisch. 118/118 CTest und 6/6 unabhängige Python-Regressionstests PASS.
- H1-H4-RMSE neu 0,120271/0,120037/0,160737/0,202501 m; zuvor 0,120676/0,117172/0,161975/0,205690 m. H2 verschlechtert sich, alle vier überschreiten die feste 0,05-m-Grenze. **Experimentelle Validierung FAIL.** Keine 6-s-Neuberechnung, kein neues feines Gitter, keine spannungskonsistente freie Oberfläche oder Zweiwege-FSI.
- Belege: docs/P2_MARIN_HEIGHT_INTERPOLATION_PARTIAL.md, scripts/p2_marin_height_interp_partial_acceptance.py, ENGINE TEST STUBE/runs/p2_marin_height_interp_partial.json mit Quell-/Native-/Testlog-Hashes. **P2 INCOMPLETE, ALL_COMPLETE=False.**

## P2 Teilnachweis: MARIN-Wandnormaldruck, negative Messvalidierung (01.11.2026)

- Opt-in `P2MarinWallNormalPressure3D`: tangential bilineare
  MAC-Druckauslesung aus zwei flüssigkeitsseitigen Normalebenen und
  lineare Wand-Rückextrapolation, mit explizitem Trocken-/Solid-Fallback.
  Keine Änderung am 3D-MAC/VOF-/PCG-Live-Solver.
- Native MSVC Release: **109 Assertions PASS**, **119/119 CTest PASS**;
  ein realer neuer 200-Schritt-/2,00-s-3D-MAC/PCG-Lauf (32×10×10).
  Gegenüber der bestehenden PCG-Kontrolle sind alle H1–H4,
  Wasser-Masse, Divergenz und nasse Sondenanzahl exakt unverändert.
  Sechs korrumpierte Traces wurden unabhängig abgewiesen.
- **Negativer Messbefund:** alle acht P1–P8-RMSE verschlechtern sich
  gegenüber dem vorherigen tangentialen Druck-Proxy, etwa P1
  4550,48→4617,76 Pa und P5 4242,11→4996,82 Pa.
  **Kein** experimenteller Druck-PASS, keine 6-s-/Dreigitter-
  Konvergenz und keine physikalische Wandtraktions-/FSI-Abnahme.
- Belege: `docs/P2_MARIN_WALL_NORMAL_PARTIAL.md`,
  `ENGINE TEST STUBE/runs/p2_marin_wall_normal_partial.json`,
  `scripts/p2_marin_wall_normal_build.cmd`. Original-MARIN-Archiv
  unverändert. `phase_gate.py`: P2 **PARTIAL**, ALL_COMPLETE=False.


## P2 Teilfortschritt: geometrische MAC-PCG Wasser-Luft-Druckgrenze (01.11.2026)

- Opt-in vorgegebene Grenzdistanz theta*dx (0<theta<=1) in allen drei
  staggered MAC-Face-Richtungen. Konsistente PCG-Poisson-Diagonale und
  Druckgradient; Default unveraendert, Zusatzspeicher nur bei Nutzung.
- Reale MSVC-Release-Abnahme: 21 hergestellte 3D-Referenzfaelle,
  5^3/9^3, x/y/z, theta=0.25/0.5/0.75, gespiegelte Oberflaeche;
  3209 native Assertions, 120/120 CTest PASS, 4/4 mutierte QA-Logs
  verworfen. Belege/Hashes in docs/P2_FRACTIONAL_FREE_SURFACE_PCG_PARTIAL.md
  und ENGINE TEST STUBE/runs/p2_fractional_surface_partial.json.
- P2 weiterhin PARTIAL: keine automatische VOF/PLIC-Geometrie,
  keine stresskonsistente Zwei-Phasen-Grenze, keine neuen >=6s
  MARIN-3-Gitter-Messnachweise, keine bidirektionale dynamische FSI
  oder physikalische Gesamtenergiebilanz.
- phase_gate.py: GATES_PASS=P0, GATES_INCOMPLETE=P1,P2,P3,P4,P5,P6,P7,P8;
  ALL_COMPLETE=False. Historisches P1-Quellhashproblem bleibt offen.

## P2 Teilfortschritt: echte VOF-Achsgeometrie an MAC-PCG-Druckgrenze (01.11.2026)

- Die vorhandene 3D-VOF-Normalenrekonstruktion wird mit dem opt-in
  geometrischen MAC-PCG-Grenzabstand verbunden, **nur** für streng
  achsparallele planare Wasser-Luft-Schnitte mit Nassschwelle 0,5.
  Aus f=0,75 (nasse Teilzelle) folgt theta=0,25; aus f=0,25
  (trockene Teilzelle) theta=0,75. Keine VOF-als-Distanz-Heuristik.
- Ungültige/oblique/doppelt partielle Grenzflächen werden abgewiesen,
  reine 1/0-Zellgrenzen als geometrisch unaufgelöst gekennzeichnet.
  Der MAC-Druckprojektor kann alte Face-Distanzen vor dem nächsten
  Frame löschen; unabhängiger Zwei-Frame-Drucktest verifiziert Reset.
- Echte MSVC-Release-Prüfung: 24 hergestellte 3D-Referenzfälle
  (5^3/9^3, x/y/z, beide Orientierungen, zwei Fülllagen),
  **5171 native Assertions, 121/121 CTest PASS**. Vier korrumpierte
  Native-Traces durch Python-QA abgewiesen, Quell-/Binär-/Log-SHA256.
- Negative externe MARIN/SPHERIC-Druckvalidierung bleibt bestehen:
  P1-P8 RMSE ca. 4,3 bis 5,0 kPa. Keine neue teure 6-s-Simulation,
  keine 3-Gitter-Messkonvergenz, kein Live-VOF-Schwellwertwechsel,
  keine spannungskonsistente Luft-/FSI-/Energie-Vollkopplung.
- Reproduktion und Belege: docs/P2_AXIS_VOF_PCG_PARTIAL.md,
  scripts/p2_axis_vof_pcg_partial_acceptance.py,
  ENGINE TEST STUBE/runs/p2_axis_vof_pcg_partial.json.
  Phase-Gate bleibt **P2 PARTIAL / ALL_COMPLETE=False**.
