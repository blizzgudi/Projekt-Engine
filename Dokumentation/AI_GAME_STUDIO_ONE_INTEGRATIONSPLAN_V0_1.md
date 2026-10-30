# AI Game Studio One - konkreter Integrationsplan v0.1
Stand: 2026-10-26
Status: ENTWURF / keine Runtime-Integration und kein neuer Testlauf durch dieses Dokument.

## 0. Quellen / Live-Abgleich
- AI Game Studio: `D:\AI-Game-Studio`, Produktionshost 127.0.0.1:8775 im Live-Check erreichbar, Prozess `Studio.Tools.production_studio`.
- Eigene Engine: `D:\Projekt Engine\Engine Core\UnifiedPhysicsCore`, C++17-Physikkern; bestehende `MaterialLibrary` / `VoxelGrid` in `include/upc/core.hpp`.
- Engine-Pruefbericht `ENGINE TEST STUBE/runs/unified_physics_gate.json`: P0-P6 PASS als jeweils begrenzte CPU-MVPs, P7/P8 PARTIAL. Diese Statuswerte sind Report-Evidence, keine neu ausgefuehrte Simulation.
- Produktionspfad Studio: `Studio/Core/production_asset_stage.py`, `production_asset_exchange.py`, `production_inspected_asset_transfer.py`, `production_blender_visual_qa.py`, `production_visual_qa_review.py`.
- Konzept: `AI_Game_Studio_Material_Oekosystem_Engine_Konzept_v0_1.pdf` (18.10.2026), insbesondere Engine-Bridge, hierarchische Materialstruktur, nur lokale Aktivierung, skalierte Tests.

## 1. Ziel und strikte Designregeln
Ein Prompt oder kuratierter Bauplan erzeugt ein wiederverwendbares Asset-Prototyp mit Material-/Volumenreferenzen, Oberflaechenbeschreibung, Behavior-Spezifikation und getrennten Spielinstanzen.
1. **Keine Neuimplementierung des UnifiedPhysicsCore** und keine zweite Materialwahrheit in AI Game Studio. Engine bleibt eigenstaendig.
2. **Material = geteilter unveraenderlicher Definitionseintrag** (stabile ID, Revision/Hash, SI-Einheiten); in Voxeln nur Material-ID sowie notwendiger Zellzustand.
3. **Asset-Prototyp = geteilter Bauplan + wiederverwendete Daten**. Identische Generierungs-Inputs haben identischen Fingerprint und koennen Cache/Geometrie/Chunk-Daten teilen.
4. **Asset-Instanz = Referenz auf Prototyp + Transform + minimaler Laufzeitzustand**. Lokale Brueche, Temperatur, Feuchte, individuelle Lackkratzer werden nur als Deltas gespeichert (copy-on-write), andere Instanzen bleiben unveraendert.
5. **Nicht jedes Objekt voll voxelisieren**. Ferne/unbeschaedigte Objekte koennen statische Meshes verwenden; Volumeninformationen werden lokal bei Bedarf aktiviert.
6. **Oberflaechen getrennt von Voxelauflosung**. Lack, Rinde, Blattdetails als verlinkte Schicht-/Shaderdefinition; Volumenmodell nur bei mechanisch relevanter Dicke.
7. **Fail-closed bei fehlenden IDs, Hash-Mismatch, ungueltigen Einheiten oder nicht unterstuetzten Solveranfragen**. Keine verdeckten Annahmen als wissenschaftlich validierte Messwerte ausgeben.
8. **Studio-Host bleibt ausfuehrende Instanz**. Director beauftragt ueber bestehende persistente Goals/Tasks; Engine-Bridge ist Adapter, kein zweiter Orchestrator.
9. **Bestehende produktive Studio-Visual-QA nicht beeintraechtigen**. Neuer Integrationspfad initial in isoliertem Testfall, vorhandene Regressionen erhalten.

## 2. Datenvertrag v1
Die folgenden Datensaetze werden versioniert, schema-validiert und nach canonical JSON / binaerem Payload gehasht:
- `MaterialDefinition`: `material_id:uint32`, `revision`, `phase`, `density_kg_m3`, `viscosity_pa_s` (bestehende Kernfelder), erweiterte typisierte Profile fuer Thermik, Elastizitaet, Bruch, Porositaet oder Biologie. Fehlende Parameter nicht raten: validierte Presets oder ausdruecklich als Annahme markierte Werte.
- `AssetBlueprint`: `asset_id`, `schema_version`, `generator_type`, `seed`, `dimensions_m`, `material_refs`, `region_rules`, `coating_refs`, `behavior_refs`, `voxel_policy`, `source_refs`.
- `VoxelPrototype`: `blueprint_sha256`, `material_registry_snapshot_sha256`, `generator_version`, `cell_size_m`, `chunk_refs`, `mesh_refs`, `bounds_m`, `mass_kg`, `artifact_hashes`.
- `AssetInstance`: `prototype_ref`, `transform`, `state_version`, `dirty_chunk_deltas`, `coating_damage_deltas`, `simulation_state_ref`.
- `SurfaceCoating`: `coating_id`, `material_ref`, `surface_selection`, `thickness_m` optional, `pbr_profile_ref`, `damage_rule_ref`; keine Grundlack-Schichten als Millionen Mikrovvoxel.

## 3. Integrationsgrenzen
- `AI Game Studio (Director -> TaskSystem -> Agents/Skills -> Verification)`
- `Studio/EngineBridge` (neuer Adapter): `validate_blueprint`, `resolve_material_refs`, `compile_volume`, `build_surface`, `run_physics_case`, `export_artifacts`, `collect_evidence`. Jede Operation mit Goal-ID, Version, Input-/Output-Hashes und typisiertem Ergebnis.
- `Projekt Engine/UnifiedPhysicsCore`: Eigentum an Material-IDs, Voxelzustand, numerischer Berechnung und Solver-Erweiterungen; C++17 als eigenstaendige Bibliothek.
- `Blender/Unreal` als getrennte Adapter fuer Darstellung, Autorenschaft und spaetere Runtime. Erster Test braucht **kein** Unreal PIE.
- Die Studio-Agenten generieren **Datenvertraege und Parameter**, nicht beliebige native C++-Aufrufe; Native-Ausfuehrung nur in bestehenden kontrollierten Studio-Worker-Grenzen.

## 4. Umsetzungsreihenfolge mit echtem Nachweis

### Meilenstein A - Referenzen und Datenvertrag
- Schemas und Validatoren fuer MaterialDefinition/AssetBlueprint/Prototype/Instance/SurfaceCoating.
- Abbildung der externen Material-UUID/Revision auf die bestehende `upc::MaterialLibrary`-ID; stabile Zuordnung und Kollisionspruefung.
- Unit-Tests fuer unbekannte Material-ID, falsche Revision, fehlende Phase, ungueltige Maße, NaN, Pfad-Sicherheit und deterministische Fingerprints.
- **PASS wenn:** Ein Asset kann Materialdefinitionen referenzieren, ohne Materialdaten zu kopieren; 100 Prototyp-Instanzen verweisen nachweisbar auf dieselbe Material- und Geometrieversion.

### Meilenstein B - Minimales virtuelles Drucken (isoliert)
- Generator einer kleinen dreischichtigen Platte mit Hohlraum und definierter grober Aufloesung.
- Deterministische Regionenzuordnung -> belegte Material-IDs im vorhandenen VoxelGrid; Chunk-/Prototyp-Caching auf Basis von Fingerprints.
- Exakte SI-Maße, Volumen, Massenauswertung mit `VoxelGrid.mass_kg`; kein 3D-Druckprozess in Echtzeit erforderlich.
- **PASS wenn:** reproduzierbarer Output, Nachweis ueber Materialanteile/Masse, gleiches Blueprint erzeugt gleichen Prototyp-Hash und 100 Instanzen speichern keine 100 Materialdefinitionen.

### Meilenstein C - Lack/Oberflaeche & lokal beschraedigte Instanz
- SurfaceCoating separat vom dichten Volumen; Material-Shader/UV- oder Triplanar-Mapping nur fuer Rendering.
- Ein gezielter Schaden erfasst betroffene Voxel/Regionen und lokales Oberflaechen-Delta; andere Instanzen unveraendert.
- Blender-Vorschau und sechs Kamerabilder ueber erlaubte Studio-Host-Pipeline; technische QA getrennt von Bildurteil.
- **PASS wenn:** intakte und beschaedigte Instanz visuell getrennt; Referenz-Sharing intakt; CPU-Masse/Schaden nachvollziehbar, keine Behauptung zu unvalidierter vollstaendiger Fluid-/Bruchkopplung.

### Meilenstein D - Organischer Generator (Baum)
- Parametrischer Branch-Graph + Seed, Stamm und starke Aeste als mehrfach nutzbarer volumetrischer Kern.
- Kleine Aeste und Blaetter als Mesh-Instanzen, Rinde als Oberflaechenmaterial; kein Blatt-Voxel-Zwang.
- Nur zusaetzlich erforderliche Zonen fuer Wind/Bruch/Feuchte/Wachstum aktivieren; Wachstum ist ein separates regelbasiertes Modul, aktuell NICHT aus dem vorhandenen Engine-Core bewiesen.
- **PASS wenn:** 3 reproduzierbar unterschiedliche Baeume derselben Art denselben Materialkatalog nutzen; lokaler Astbruch veraendert nur eine Instanz; keine ungemessene AAA-Leistungsbehauptung.

### Meilenstein E - Direktor/QA und Runtime
- Studio-Tasks mit EngineBridge-Abhaengigkeit, saubere Resume/Retry-Fingerprints, strukturierte Fehler und unabhängige QA.
- Visual QA FAIL -> Direktor -> zielgerichtete Research-/Skill-/Blueprint-Revision -> isolierter neuer Versuch; nur gepruefte Aenderung promoten.
- Danach Unreal-Adapter (P8) und Leistungsgates fuer sparse Chunks/LOD/GPU (P7).
- **PASS wenn:** nachvollziehbare native End-to-End-Laufkette vom Studio-Goal bis zu echtem Render/Test; keine Unit-Test-Verwechslung mit Gameplay- oder Runtime-Fertigstellung.

## 5. Sofortiger erster Arbeitsauftrag
1. In **separatem Testpfad**, nicht am laufenden Produktionshost 8775, den v1-Datenvertrag und Referenzvalidierung implementieren.
2. Minimalplatte mit 3 zentral registrierten Materialien und 2 Oberflaechenschichten als Blueprint definieren.
3. Einmal prototypisieren; 100 Instanzen auf denselben Prototyp referenzieren.
4. Eine Instanz lokal beschaedigen und Masseneinschraenkungen, Prototyp-Sharing, Deltas und Hashes pruefen.
5. Beweiskette (Quellenrevision, Testbericht, ggf. Blender-Vorschaubilder) aufzeichnen und erst dann in den Studio-Agentenworkflow einklinken.

## 6. Nicht als fertig behaupten
- Kein fertiger Virtual Voxel Printer.
- Kein fertiges erweitertes persistentes Material-/Asset-Registry-Schema ueber beide Repos.
- Kein vollstaendiger dynamischer Baum-/Oekosystem-Solver.
- Kein gemessener GPU-/HLOD-/PIE-End-to-End-Beweis.
- Dieses Dokument ist Planung. Es veraendert weder den Studio-Laufzeitprozess noch vorhandene C++-Solver.

## 7. Produktentscheidung D-ONE-001 (2026-10-26): Spieleentwicklung zuerst
**Entschieden:** AI Game Studio One wird vorrangig eine integrierte, KI-gesteuerte Spieleentwicklungsplattform (Richtung 1), nicht jetzt eine universelle wissenschaftlich-technische Simulationsplattform (Richtung 2).

Prioritaeten:
1. Virtual Voxel Printer als eigenstaendigen, ueber Schnittstellen integrierbaren Asset-Generator entwickeln.
2. Eigenen UnifiedPhysicsCore und gemeinsame Material-/Voxelreferenzen fuer spielrelevante Physik verwenden.
3. AI Game Studio Director/AgentFactory/Skills/Visual QA und Werkzeuge zu einem zusammenhaengenden Produktionsprozess verbinden.
4. Spielbereite Assets und Weltbau, Performance, Editor-Benutzbarkeit, Unreal-Adapter und langfristig eine eigene Game Runtime nach und nach umsetzen.
5. Keine universellen technischen Konstruktionstools, wissenschaftlichen Genauigkeitsgarantien oder beliebigen Material-/Chemiesolver als Pflicht fuer V1.

**Option fuer spaeter:** Architekturgrenzen, versionierte Datenformate und unabhaengige Module so gestalten, dass ein Simulation Mode spaeter moeglich bleibt. Er wird nur angegangen, wenn die Spieleplattform nachweislich stabil laeuft, ein konkreter Bedarf entsteht und Kosten/Nutzen positiv ausfallen. Nicht vorsorglich bauen.

Diese Entscheidung ist Produkt-Scope, **kein Nachweis** fuer neue Runtime-Funktionen und keine Freigabe zum Umbau laufender Tests/Host 8775.
