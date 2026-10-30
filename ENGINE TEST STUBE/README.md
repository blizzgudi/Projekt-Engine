# ENGINE TEST STUBE

Eigenstaendiges Labor fuer experimentelle Engine-Bausteine. Bestehende Unreal- und AI-Game-Studio-Projekte bleiben getrennt.

## Struktur
- experiments/: isolierte Experimente mit Code, Tests, Ergebnissen und Dokumentation
- benchmarks/: reproduzierbare lokale Leistungswerte
- runs/: protokollierte Testlaeufe inkl. Editor-Protokollen
- scripts/run_all.py: zentraler Einstiegspunkt fuer automatische Experimenttests
- docs/TEST_PROTOCOL.md: Regeln fuer aussagekraeftige Ergebnisse
- lab.json: maschinenlesbarer Experimentkatalog
- unreal/ENGINE_TEST_STUBE/ENGINE_TEST_STUBE.uproject: eigenes Unreal Engine 5.8 Projekt

## Start - automatische Tests

    cd "D:\Projekt Engine\ENGINE TEST STUBE"
    C:\Python314\python.exe scripts\run_all.py

## Start - Unreal

    D:\Projekt Engine\ENGINE TEST STUBE\unreal\ENGINE_TEST_STUBE\ENGINE_TEST_STUBE.uproject

Der technische UE-Projektname verwendet Unterstriche, weil der Projektbezeichner keine Leerzeichen enthalten sollte. Im Editor ist die Projektbeschreibung ENGINE TEST STUBE hinterlegt.

## Experimente
1. voxel_water_001: Wasser-/Wand-Materialreferenzen, diskrete Zellenbewegung, Datenerhalt, 3 OBJ-Exports und Schnittbild. Keine echte Fluidphysik.

## Evidenz
- Python-Unit-Tests/Simulation/CPU-Benchmarks: PASS
- Unreal 5.8: erster Startversuch erreichte laut Log die Engine-Initialisierung und den Quit-Befehl, aber der Prozess beendete sich nicht innerhalb von 75 Sekunden und wurde abgebrochen. Kein vollstaendiger Unreal-Start/Shutdown-PASS.
- Ein erfolgreicher CPU-Test beweist kein Unreal-Rendering, keine realistische Wasserphysik und keine AAA-Qualitaet.
