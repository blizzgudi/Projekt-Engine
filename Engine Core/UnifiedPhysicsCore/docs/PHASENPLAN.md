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
