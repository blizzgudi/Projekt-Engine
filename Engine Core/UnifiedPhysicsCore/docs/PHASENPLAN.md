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
