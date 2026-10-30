# Virtual Voxel Printer – erster Baumstamm-Materialdruck

**Status:** UPC-basiertes, isoliertes Offline-Proof-of-Concept – kein Blender/UE native E2E, keine vollständige VVP-App.

**Konzept:** Ein 1,344 m hoher Stamm mit prozeduralen Bark-Grooves und Stammfuß-Auswölbungen wird von unten nach oben mit Materialvoxeln befüllt. Die Voxeldaten stammen aus einer echten C++17-Ausführung der vorhandenen \`upc::MaterialLibrary\` und \`upc::VoxelGrid\`.

## Öffnen

**Interaktive Vorschau:** \`viewer.html\` doppelklicken. Oben ist ein Schicht-Regler, rechts kann zwischen Volldruck, aufgeschnittener Stamm und Querschnitt gewechselt werden.

**3D-Meshes:** \`output/tree_trunk_full.obj\` und \`output/tree_trunk_cutaway.obj\` (jeweilige .mtl daneben) können in Blender oder andere OBJ-Viewer importiert werden. Das Mesh wurde aus den belegten Voxel-Grenzflächen extrahiert, noch nicht geglättet/optimiert.

**3D-Volumen:** \`output/voxel_material_ids.bin\` (z,y,x, 72 x 72 x 112, uint8, 12 mm Zellkante). 0=leer, 1=Kernholz (UPC 1101), 2=Splintholz (1102), 3=Kambium (1103), 4=Rinde (1104). Der Bericht \`output/report.json\` nennt die Test-Dichten und Massen.

## Reproduzierbar ausführen

1. Im Visual Studio 2022 x64 Native Tools Command Prompt:
\`\`\`bat
cl /std:c++17 /EHsc /O2 /I "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\include" /Fe:"trunk_printer.exe" "trunk_printer.cpp"
trunk_printer.exe output
\`\`\`
2. \`C:\Python314\python.exe visualize_tree.py\`
3. \`C:\Python314\python.exe test_tree_print.py\`

Beteiligter Physics-Core ist in \`D:\Projekt Engine\Engine Core\UnifiedPhysicsCore\include\upc\core.hpp\`. Andere vorhandene Studio/Engine-Dateien wurden nicht geändert.

## PASS-Evidence

- Echte C++17-UPC-MaterialLibrary und VoxelGrid; Masse \`VoxelGrid.mass_kg\`, Vergleich mit unabhängig summierten Zellmassen.
- Vier physische Materialregionen (Kernholz, Splintholz, Kambium, Rinde), getrennte IDs; idempotente Parameter und deterministische Materialbelegung.
- \`output/render_validation.json\` verifiziert Materialzählungen, SHA256 des binären Volumens, Mesh-Quads und erzeugte Bilder.
- Fünf Python-Unittests erfolgreich.
- \`output/tree_trunk_overview.png\`, \`tree_trunk_print_progress.png\`, \`tree_trunk_cross_section.png\` und \`tree_trunk_stage_*.png\` sind Rasteransichten aus dem ECHTEN exportierten Voxelvolumen.
- \`output/tree_trunk_full.obj\` und \`tree_trunk_cutaway.obj\` sind exportierte facettierte Mesh-Oberflächen.

## Ehrliche Grenzen

- Holz-Dichten sind illustrative Test-Parameter; keine Messung realer Holzarten. Kambium-Dicke für Auflösung vergrößert.
- "Virtueller Druck" bedeutet materialweise Volumenbelegung und schichtweise Präsentation, nicht physikalisch simulierte Schmelz-/Extrusions-Druckmechanik.
- Der Volumen-Solver rechnet hier **keinen** Bruch, Baumwachstum, Zelltransport, Holzfeuchte oder Baum-Biologie durch; geprüft sind Material-IDs und Masse. Der UPC hat andere getrennt getestete Solver.
- Weder Blender- noch Unreal-Laufzeit und keine Studio-Host-Orchestrierung wurden für diesen Proof benutzt; deren Native E2E gilt als NOT_RUN.
- Die sichtbaren Rindenrillen sind geometrisch, nicht fotorealistische Texturen. Eine reale VVP-Anwendung braucht Mesh-Smoothing, UV/Materials, LOD und Asset-Registry; fortgeschrittene biologische Modelle optional.
