# P8 – Unreal Engine 5.8 Native-Adapter und Render-/PIE-E2E-Abnahme

Stand: 2026-10-26.
Projekt: D:\Projekt Engine\ENGINE TEST STUBE\unreal\ENGINE_TEST_STUBE

## Abnahmeumfang und nachgewiesene Umsetzung

Ein **einziger** Unreal-5.8-Runtime-Plugin-Adapter \`EngineMaterialLab\` nutzt
den unveränderten unabhängigen Unified Physics Core als Header-Only-C++-
Bibliothek (u. a. \`core.hpp\`, \`mac3d.hpp\`, \`coupled_water3d.hpp\`,
\`fluid3d_module.hpp\`). Keine Unreal-Engine-Quellcodeänderungen und
kein Zugriff auf das unabhängige AI-Game-Studio-Projekt.

Die P8-Runtime verwendet **echten** \`UnifiedPhysicsRuntime\` und seinen
\`CoupledMacWater3D\` statt der alten 2D-Simulation:
- 12 x 8 x 6 Fluidzellen à 0,1 m.
- Anfangswassermasse exakt 128 kg bei rho=1000 kg/m³.
- 3D-Dammbruch-Simulation mit lokalem Schwerkraft-/Druckprojektion-Step.
- Native \`UInstancedStaticMeshComponent\` rendert sämtliche Wasserzellen
  mit physikalisch berechnetem VOF-Anteil als dynamische Engine-Cubes.
- Native \`ASceneCapture2D\` + \`UTextureRenderTarget2D\` erzeugt
  eine echte UE-GPU-gerenderte PNG-Datei mit 960 x 540 Pixeln.
  PNG ist **kein** synthetisches Offline-Plot oder nachträglich
  konstruiertes Bild.
- C++ native Runtime protokolliert Framezahl, Initial-/Endmasse,
  maximale Massenerhaltungsabweichung, Wasservolumen außerhalb
  der ursprünglichen Dammregion, dynamische Renderinstanzen und
  das erfolgreich exportierte PNG.
- UE5.8-Editor-Start und unabhängig gestarteter Game-Prozess sowie
  **echte PIE-Sitzung** (Log "Created PIE world") werden geprüft.
  Eigenständige UE-Prozesse müssen mit ExitCode 0 sauber enden;
  frühere Crashfälle wurden behoben.
- Native MSVC/UBT-Compilation für Win64 UnrealEditor Development,
  UnrealGame Development und UnrealGame Shipping im isolierten
  \`RunUAT BuildPlugin\` erfolgreich. Das Paket liegt unter
  \`ENGINE TEST STUBE/unreal/builds/EngineMaterialLab_P8/\`.

## Prüfung reproduzieren

UE Editor 5.8: C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe

Mit Windows Python 3.14:

\`\`\`powershell
$python = 'C:\Python314\python.exe'
$root = 'D:\Projekt Engine'
& $python "$root\ENGINE TEST STUBE\scripts\p8_native_e2e.py" game
& $python "$root\ENGINE TEST STUBE\scripts\p8_native_e2e.py" pie
& $python "$root\Engine Core\UnifiedPhysicsCore\scripts\p8_acceptance.py"
& $python "$root\Engine Core\UnifiedPhysicsCore\scripts\phase_gate.py"
\`\`\`

Die Runtimes starten nur \`ENGINE_TEST_STUBE.uproject\` und nutzen
\`/Engine/Maps/Entry\`; Level-/Engine-Dateien anderer UE-Projekte
werden nicht geändert. Game-Lauf wird per dedizierter Autoquit-Flag
nach gültiger Native-Capture beendet. PIE wird durch das eigene
\`Content/Python/p8_pie_probe.py\` als echte Simulate-in-Editor-Sitzung
gestartet, **zuerst ordnungsgemäß beendet** und erst dann wird
der Editor selbst beendet. Kein vorgetäuschter PIE-Screenshot.

Reale Rohartefakte in
\`ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Saved/EngineMaterialLab/P8Native/\`:
- P8_GAME_UE58_Native_3DWater.png/.json
- P8_PIE_UE58_Native_3DWater.png/.json

Gesicherte Prozessbelege (ExitCode, Log, Zeit, PNG-Merkmale, SHA256):
- \`ENGINE TEST STUBE/runs/p8_game_process.json\`
- \`ENGINE TEST STUBE/runs/p8_pie_process.json\`
- \`ENGINE TEST STUBE/runs/p8_game_unreal_native.log\`
- \`ENGINE TEST STUBE/runs/p8_pie_unreal_native.log\`
- \`ENGINE TEST STUBE/runs/p8_acceptance.json\`

Das eigenständige P8-Phase-Gate muss ECHTE PNG-Bytes sowie
Unreal-Prozesscode 0, Game+PIE-Logmarker, keine Fatal/Assertion,
aktuelle Plugin-Binaries, Quellen-/DLL-SHA256 und positive
3D-Wassermengen-/Massenprüfungen nachweisen. \`UnrealBuildTool\`-
und \`AutomationTool\`-Ergebnisse sind real lokal vorhanden.

## Ehrliche Grenzen

**P8-PASS bedeutet einen nachgewiesenen Unreal-5.8-Native-Adapter-E2E
für das 3D-MAC-Fluid und die Build-/Rendering-/PIE-Verbindung.**

- Es ist noch keine fertige eigenständige AAA-Engine, kein
  vollständiges UE-Gameplay/Material-Ökosystem und keine
  visuelle Vollintegration ALLER P3–P6-Subsysteme.
  Der Adapter rendert und simuliert aktuell die P2-3D-Flüssigkeit.
- Keine Framerate-/LOD-/GPU-Compute-Skalierungsabnahme; das gehört P7.
- Eigenes CPU-VOF mit InstancedStaticMesh ist experimentell,
  kein Niagara- oder GPU-Fluids-Ersatz und keine fotorealistische
  transparente Flüssigkeitsoberfläche.
- PIE-Screenshot ist eine externe \`SceneCapture2D\` aus einer
  realen PIE-World, nicht direkt ein Nutzerviewport-Screenshot.
- Die Runtime-Demonstration erzeugt ihre Szene dynamisch auf
  der Engine-Entry-Map; kein vollständiges Spiel-Level/Map-Cook.
- Simulation und Rendering laufen im normalen UE-5.8-
  Editor-/Game-Prozess mit der Projekt-Plugin-DLL; kein nachgewiesener
  standalone gekochter Shipping-Spielbuild, obwohl der Plugin-
  Shipping-UBT-Kompilationsschritt erfolgreich ist.

## Nachtrag 26.10.2026: Live-viskoser 3D-MAC/VOF-Pfad in Unreal 5.8

Nach Einführung des optionalen `CoupledMacWater3D`-Viskositätsschritts
wurden die originalen UPC-Header einschließlich
`viscous_mac3d.hpp` bytegleich in den isolierten
`ENGINE_TEST_STUBE`-Pluginadapter übernommen.
Die P8-Demo aktiviert **nu=0,005 m²/s** (illustrativer Testwert,
nicht reale Wasserviskosität) und protokolliert die tatsächlich
durchlaufenen viskosen Substeps.

**Echte Native-E2E-Neuabnahme im isolierten UE5.8-Projekt:**
- UAT BuildPlugin Win64 UnrealEditor Development, UnrealGame
  Development und UnrealGame Shipping: **BUILD SUCCESSFUL**.
- Zwei getrennte echte UE5.8-Prozesse: **GAME PASS, Exit 0**
  (32,34 s); **PIE PASS, Exit 0** (27,4 s).
- 85 Simulationsframes, jeweils **111 viskose interne Substeps**.
- 128 kg Wasser vor und nach der Simulation; Wasser außerhalb
  des ursprünglichen Damms: max VOF 0,467519769.
- Reale SceneCapture-PNGs 960×540:
  GAME SHA256 `c920703a156f8c6143aa77c1cb717ef4eec3a4ac934d9729a73a91ae48b97efc`;
  PIE SHA256 `34ff2dcdc5f858409a38fb0b48bf8746c366e14194461e3c438a6815efcc4cbf`.
- Native-Prozessberichte und Logs unter
  `ENGINE TEST STUBE/runs/p8_game_process.json` und
  `p8_pie_process.json`; Gate `scripts/p8_acceptance.py`
  prüft den aktiven viskosen Pfad ausdrücklich.

**Grenzen:** Visuelle Würfel-Demo statt hochwertigem Wasser-Shader;
keine beweglichen Körper mit Impulsrückgabe, keine P3–P6-
Materialsimulation in UE, keine P7-RDG/LOD-Integration,
keine wissenschaftlich validierte 3D-Strömungsreferenz.
P8 bleibt **PARTIAL** im Vollphasen-Gate.
