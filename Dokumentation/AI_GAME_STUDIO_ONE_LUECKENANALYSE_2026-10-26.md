# AI Game Studio One – Lückenanalyse und priorisierter Ergänzungsplan
Stand: 2026-10-26
Status: Architektur-Audit / keine Umsetzung oder Runtime-Abnahme.
Produktentscheidung: Spieleentwicklungsplattform zuerst; universelle wissenschaftliche Simulation nur spätere Option.

## Geprüfte Quellen
- D:\Projekt Engine\Dokumentation\AI_GAME_STUDIO_ONE_INTEGRATIONSPLAN_V0_1.md (v0.1 mit D-ONE-001)
- D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\docs\PHASENPLAN.md
- D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\docs\P7_SPARSE_LOD_PARTIAL.md
- D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\docs\P8_UNREAL58_NATIVE_E2E_ABNAHME.md
- D:\Projekt Engine\ENGINE TEST STUBE\runs\unified_physics_gate.json
- D:\AI-Game-Studio\Studio\Core\production_asset_stage.py, production_asset_exchange.py, production_inspected_asset_transfer.py, production_visual_qa_review.py
- AI_Game_Studio_Material_Oekosystem_Engine_Konzept_v0_1.pdf, 18. Oktober 2026: Zielarchitektur und Risiken, insbesondere Materialhierarchie, humanoide Referenz, Runtime/Multiplayer und Bridge.

## Tatsächlicher Kernstand – streng abgegrenzt
1. UnifiedPhysicsCore: P0–P6 jeweils begrenzte native CPU-MVP-PASS laut aktuellem Gate.
2. P7: PARTIAL. CPU-Sparse-Brick-LOD-Snapshots vorhanden, GPU/Streaming/Performance-Nachweis offen.
3. P8: PASS **nur** als UE5.8-Plugin-E2E für P2-3D-MAC/VOF-Wasser mit realen Game-/PIE-Rendern und Prüfevidence. Keine komplette Game Engine und noch keine P3–P6-Vollintegration in UE-Szenen.
4. AI Game Studio: Studio-Produktionsmodule, Asset-Handoff, Blender-Visual-QA und ChatGPT-Handoff-Module vorhanden. Das beweist NICHT den kompletten AI-Game-Studio-One-E2E-Loop.
5. Virtual Voxel Printer: bisher keine vollständige funktionsfähige App/Produktionspipeline nachgewiesen.

## PRIORITÄT P0 – Ohne das scheitert der erste echte Drucker
### P0.1 Canonical Asset Graph / Datenverträge
- Material-IDs und Versionen, Baupläne, Volumen/Chunk-Prototypen, Mesh- und Surface-Refs, Instanz-Zustandsdeltas, Source-/License-Provenance.
- Definiertes Eigentum: UnifiedPhysicsCore besitzt numerische Materialwahrheit; Printer verwaltet Bauplan und abgeleitete Artefakte; Studio besitzt Task-/QA-Evidence.
- Import-/Export-Version, Schema-Migration, Hashes, Einheiten; keine zweite Materialbibliothek.
- Abnahme: 100 Instanzen teilen Prototyp + Materialdefinition; Copy-on-Write-Schaden an einer Instanz.

### P0.2 Voxel -> nutzbares Game-Mesh
- Oberflächenextraktion, Normalen, UV oder Triplanar-Materialmapping, Material-Slots, Kollisionsrepräsentation und skalengerechte Exporte.
- Nicht jeden Voxel als sichtbaren Würfel rendern! Querschnitt/Inspektor bewusst separat.
- Abnahme: geschlossenes, sichtbares Multi-Material-Objekt in echtem Viewport; korrektes Maß und Material, Kollisionsprüfung.

### P0.3 Drucker-Editor und Speicherlogik
- Eigener Viewport, Kamera, Auswahl, Materialpalette, Druck-Auftrag, Fortschritt, Abbruch, Undo/Redo, Speichern/Laden.
- Strikte Trennung Headless Printer Core / Benutzeroberfläche / Unreal- und Blender-Adapter.
- Abnahme: `VirtualVoxelPrinter.exe` mit deterministischem Bauplan -> Ausgabe -> Wiederöffnen (nicht nur ein JSON-Manifestscreenshot).

### P0.4 End-to-End QA und Rollback
- Material-/Massenerhaltung, Geometrie, Normale/UV, Renderergebnis, einzelne Asset-Instanz, Performance, Save-Reload.
- Studio-Workflow mit eindeutiger Auftrags-ID und Source-Hashes, kontrollierten Retry/Resume, unabhängiger QA und Skill-Learning-Receipts.
- Abnahme: physikalisch und visuell belegter Druck- und Schadentest ohne Unterbrechung/Manipulation am produktiven Host.

## PRIORITÄT P1 – Vor vollwertiger Spieleproduktion gezielt schließen
1. Eigene Rendering-Strategie: zunächst bewährte Viewport-/UE-Renderer verwenden; eine eigene Grafik-Runtime erfordert Licht, PBR, Schatten, Kamera, Streaming, Transparenz und Performanceprofile.
2. Game-optimierte Asset-Cooking-Pipeline: LOD/HLOD, Mesh/Collision-Build, Instancing, Occlusion, Asset-Bundles, GPU/CPU-Budgets und Cache-Invalidierung.
3. Gameplay/Scripting: Komponenten, Events, Zustandsmaschinen, Trigger, Interaktionen, Ability/Quest/Inventory Schnittstellen; sinnvolle Engine-API für ChatGPT-generierte Logik.
4. Animation/Character Pipeline: Skelett, Rig, IK, Retargeting, Blend Trees, Cloth/Hair und Animationstests; nicht jeden Charakter voll voxelisieren.
5. Scene/World Editing: Terrain, Vegetation, Gebäude-Assembly, Prefabs, Levelstruktur, Streaming, Navigation, Beleuchtung und reproduzierbare Seeds.
6. Spielzustand: Speichern/Laden, Persistenz und Migration von Instanzdeltas; Eingabesteuerung, Kamera, Benutzeroberflächen/HUD.
7. VFX/Audio: Partikeleffekte, Wetter, Umgebungsgeräusche, adaptive Musik/Voice, Cinematics; dedizierte QA.
8. Packaging: signierte/versionsgebundene Artefakte, Game-Builds, Releases, Crashlogs, Launcher, Updates, Entwicklungs-/Spiel-Modus.
9. Datenverwaltung und rechtliche Herkunft: kommerzielle Rechte der generierten/bezogenen Assets, Modellgewichte, 3D-Datasets, Bild-/Audioquellen, automatische Herkunftsbelege.
10. Betrieb: Backup, Migrationsproben, Recovery, CI/Regression, Hardware-Benchmarking, Profiling, Schutz der laufenden Produktionsjobs.

## PRIORITÄT P2 – Nur wenn das Spiel es verlangt
- Multiplayer/Server-Autorität, Replikation und Anti-Cheat; nicht voraussetzen, sofern kein Multiplayer-MVP gefordert.
- Vollständiges Wetter-, Pflanzen- und Tierökosystem, komplexe Mechatronik, frei kombinierbare Chemie; nur in begrenzten, gameplayrelevanten Domänen.
- Eigener vollständiger Renderer und allgemeiner Editor als Ersatz für Unreal/Blender: erst nach nachgewiesenem Nutzen und Machbarkeit.
- Wissenschaftlicher Simulation Mode ausdrücklich **nicht** Bestandteil von Produkt-V1.

## Priorisierte vertikale Proof-Reihe
1. **Panzerplatte**: registrierte Materialien -> volumetrischer Druck -> Oberfläche/Lack -> 100 Instanzen -> eine lokale Beschädigung -> unabhängiges physikalisches und visuelles QA.
2. **Funktionale Kiste**: Hohlraum, Scharniere, Kollision, Öffnen, Schließen, Shader/UV, Instanzen und Save/Reload.
3. **Baum**: parametrischer Branch-Graph, Volumenkern, wiederverwendbare Zweige/Blätter, Rinde, Wind und isolierter Astbruch.
4. **Modulares Gebäude**: begehbar, passende Türen/Mechanik, LOD/Streaming, Materialalterung, importierte spielbereite Komponenten.
5. **Kleine spielbare Zone**: Landschaft + einige Assets + NPC/Interaktion + einfache Quest + Verpackung/Starttest in echter Game-Runtime.

Jeder Proof benötigt Source-/Build-Hashes, Dimensions-/Funktionstests, visuelle Belege, Performance und reproduzierbaren Save/Reload. Die nächste Stufe startet nicht aufgrund eines schönen Renderbilds allein.

## Produktentscheidung und Arbeitsgrenzen
- Fokus: AI Game Studio One als integrierte KI-Spieleentwicklungsplattform; keine zusätzliche universelle Simulationsplattform als V1-Pflicht.
- Keine Duplikation von Materialwahrheiten, Solvern oder Studio-Task-/QA-Infrastruktur.
- Physik-Core / Printer-Core / Game-Runtime / AI Director / Data Registry sind getrennte Module mit festem Ownership.
- Verworfene/fehlgeschlagene Varianten nur nach isolierten Tests und belegter Qualitätsverbesserung promoten.
- Dieses Audit fügt keine neuen Native-Aufträge ein und verändert weder laufenden Port 8775 noch bestehende Engine-/Studio-Solver.
