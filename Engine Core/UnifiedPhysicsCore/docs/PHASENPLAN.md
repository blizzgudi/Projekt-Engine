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
