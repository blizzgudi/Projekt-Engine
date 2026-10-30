# ENGINE Projektordner-Inventar
Stand: 2026-10-26

## Eigenstaendige Engine-Projektordner, auf D:\ geprueft
- D:\ENGINE TEST STUBE: zum Zeitpunkt der Pruefung vorhanden, dann nach D:\Projekt Engine\ENGINE TEST STUBE verschoben.
- D:\Voxel-Wasser-Test: nicht mehr vorhanden; war bereits zuvor in ENGINE TEST STUBE umbenannt bzw. uebernommen worden.
- D:\Projekt Engine: zum Zeitpunkt der Inventur nicht vorhanden, bei Migration neu eingerichtet.
- D:\Project Engine, D:\Engine, D:\Die Engine: nicht vorhanden.

## Andere Hauptordner auf D:\
- D:\AI-Game-Studio mit Forschungsunterlagen unter Research und eigenem Studio.
- D:\AI-Game-Studio-Checkpoints, D:\AI-Game-Studio-Evidence und D:\AI-Game-Studio-Tools sowie weitere AIGS-Temp-/Kontrollordner.
- D:\ProjectRubi mit Spielprojekten.
- Weitere nicht zur Engine gehoerende Programme und Ordner.

Diese Verzeichnisse wurden NICHT verschoben. Es wurden an deren Wurzeln keine weiteren eindeutig als eigenstaendiger Engine-Testordner erkennbaren Verzeichnisse gefunden. Die Suche war eine Ordnernameninventur auf D:\ und in den unmittelbaren Unterverzeichnissen der genannten Projektbereiche, kein vollstaendiger Laufwerksscan nach jeder Engine-Datei.

## Bisherige Engine Test Stube
- experiments/voxel_water_001: Voxelraster, Wasser-/Wand-Referenzen, vereinfachte Bewegung
- unreal/ENGINE_TEST_STUBE: Unreal Engine 5.8 Testprojekt und Startlogs
- benchmarks: reproduzierbarer CPU-Teststand
- runs: automatisierte Testreports und Historie
- scripts: Teststarter
- docs: Testprotokolle
- lab.json: Experimentkatalog

## Bekannte technische Grenzen
- Voxelwasser verwendet nur eine vereinfachte Zellbewegung, keine echte Fluessigkeitsphysik.
- Unreal-Start wurde vor Migration nicht vollstaendig abgeschlossen. Ein neuer UE-Test ist noch offen.
- Historische Protokolle duerfen alte Absolutpfade enthalten. Sie dokumentieren den damaligen Zustand.
