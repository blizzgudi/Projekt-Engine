# P2 – Exakte bewegte 3D-MAC-Dualvolumina (TEILABNAHME, 28.10.2026)

## Problem und Abgrenzung
Die zuvor nachgewiesene konservative signierte Cut-Cell-Skalaradvektion
arbeitet auf zellzentrierten Fluidvolumina. Gestaffelte MAC-Geschwindigkeiten
liegen dagegen auf Flaechen und erfordern eigene *Dual*-Kontrollvolumina.
Die naive Mittelung zweier primaler Zell-Alpha-Werte ist fuer
teilweise verdeckte Quader geometrisch nicht korrekt.

## Implementierung
include/upc/p2_moving_mac_dual_volumes3d.hpp erzeugt fuer u, v, w
jeweils ein eigenstaendiges, um dx/2 verschobenes Gitter:
u: (nx-1,ny,nz), v: (nx,ny-1,nz), w: (nx,ny,nz-1).
Fuer jedes Dualgitter werden **exakte** Quader-Schnittvolumina zum
alten/neuen Zeitpunkt und zeitintegrierte geometrische
Festkoerper-Wandvolumenfluesse mit lokalem GCL berechnet. Die
geometrischen Quellterme werden NICHT mit einem physikalischen
Fluidimpuls oder einem Wandkontaktgesetz verwechselt.

Der vorgeschriebene Quader muss an allen sechs Aussenflaechen
mindestens dx/2 Abstand halten. Rand-Halbduals sind nicht enthalten.
Eine Translation pro Schritt ist auf maximal dx je Achse begrenzt.
Keine dynamischen Starrkoerper, keine Rotationen, keine
momentumerhaltende Advektion und keine Druckprojektion auf den
Dualvolumina. Vorhandene primalen Solverpfade bleiben unveraendert.

## Reproduzierbare Abnahme
- scripts/p2_moving_mac_dual_full_build.cmd konfiguriert und baut
  MSVC-Release mit CMake/Ninja und fuehrt **alle 71 CTest-Suites** aus.
- tests/p2_moving_mac_dual_volumes3d_tests.cpp prueft 8^3, 12^3,
  16^3, alle Bewegungsachsen und Richtungen, zwei Schrittweiten,
  zwei Folgeschritte und alle drei MAC-Komponenten. Jede Dualzelle
  wird gegen eine unabhaengige direkte geometrische
  Schnittvolumenformel geprueft. Hinzu kommen lokales und globales
  GCL, unveraenderte Eingaben, stationaerer Quader, Fehlerfaelle und
  der Nachweis des Unterschieds zur falschen primalen Mittelung.
- scripts/p2_moving_mac_dual_acceptance.py validiert die nativen
  Resultate zusaetzlich gegen **216 analytisch separat berechnete
  Dual-Referenzwerte**, prueft das vollstaendige CTest-Protokoll,
  erstellt SHA-256-Quellfingerprints und einen JSON-Teilbericht.
- Protokoll: ENGINE TEST STUBE/runs/p2_moving_mac_dual_full_ctest_20261028.txt.
- Bericht: ENGINE TEST STUBE/runs/p2_moving_mac_dual_partial.json.

## Noch offen
Ein vollstaendiger, konservativer MAC-Dualvolumen-Impulstransport
braucht konsistente Fluidfluesse auf den Dualflaechen und
Rand-Impulsuebertraege an die bewegte Wand. Das geometrische GCL
allein ist kein physikalischer Impuls-/Energieerhaltungsbeweis.
Danach erst Kopplung mit primalem MAC/VOF, Druckprojektion,
freiem Wasser/Luft-Interface und dynamischer FSI.
Unabhaengige 3D-CFD-Messdaten und UE5.8-Native-E2E fuer diesen Pfad
fehlen weiterhin. **P2 PARTIAL; P3-P8 PARTIAL; ALL_COMPLETE=False.**
