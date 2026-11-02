# P2 – MARIN Live-VOF-Spalten und native Wandlast (Teilnachweis)

**2026-11-02: P2 PARTIAL. Keine physikalische Vollabnahme.**

## Reproduzierter Blocker und Implementierung

Bisher verarbeitet `P2MarinBoxWallTraction3D::audit_heightfield` nur extern vorgegebene
Wasserlinien und Zell-Druckfelder. Ein echter `CoupledMacWater3D`-Zustand
konnte nicht direkt in die geometrisch beschnittene MARIN-Fuenfflaechen-
Kraft-/Momentendiagnose eingespeist werden. Der neue
`P2MarinLiveColumnWall3D::audit_live` uebernimmt die **tatsaechlichen**
MAC-Zelldruecke, VOF-Fluessigkeitsanteile und binaeren Festkoerpermasken.

Eine explizite Spalten-Rekonstruktion akzeptiert nur Wasser, das
ueber einem durchgehenden, am Boden beginnenden Wasser-/Festkoerper-
Profil liegt, mit hoechstens einer partiellen freien Oberflaechenzelle.
Festkoerper als Bodenstuetze werden zur geometrischen Wasserlinie
hinzugerechnet, **aber nur wenn darueber wirklich Wasser vorhanden ist**:
ein trockener Festkoerper darf keine kuenstliche Dachlast erzeugen.
Nicht monotone Spalten (Spray, eingeschlossene Luft, Ueberhang),
schwebende Festkoerper, Wasser-Festkoerper-Ueberlappung und ungueltige
VOF-/Druckwerte werden abgewiesen, statt als korrekte
Wandbelastung ausgegeben zu werden. Der bisherige
`audit_heightfield`-Pfad bleibt unveraendert.

## Native Reproduktion, analytische Referenz und QA

- Vor Implementierung: MSVC C1083 fuer die fehlende Live-Bridge,
  `ENGINE TEST STUBE/runs/p2_live_column_red_build_20261102.log`.
- Nach Implementierung: nativer MSVC-Release-Build PASS,
  `p2_live_column_build_20261102.log`.
- Unabhaengige konstante-Druck-Referenz auf 32x10x10, 48x15x15,
  64x20x20 mit unveraendertem Test-02-Quader:
  bei Wasserlinie 0.10 m: benetzte Seitenflaeche
  `2*(0.161+0.403)*0.10 = 0.1128 m²`, trockenes Dach.
  Bei Wasserlinie 0.30 m: alle vier Seiten plus Dach
  `2*(0.161+0.403)*0.161 + 0.161*0.403 = 0.246491 m²`;
  Dachkraft bei 3000 Pa `-194.649 N`.
  Diese Referenzen stammen aus der analytischen Quadergeometrie,
  nicht aus dem CFD-Drucksolver.
- Separater **echter 3D-MAC-PCG-Native-Schritt** mit live Druck-/VOF-
  Feldern, 62 Assertions insgesamt, endliche Kraefte und Momente;
  maximale Projektiondivergenz im protokollierten Fall
  `9.00518e-12 s^-1`.
- Negativtests: abgetrennte Wasserinsel, nichtfinite/ueberfuellte VOF,
  schwebender Festkoerper und nichtfiniter Druck. Die unabhängige
  Python-Auditpruefung validiert zusaetzlich Native-Log, analytische
  Zahlen, CTest, SHA256 und manipulierte Reports.
- Vollstaendige bestehende Native-Regression: **155/155 CTest PASS**
  (`p2_live_column_full_ctest_20261102.log`). Keine erneuten
  teuren MARIN-6s-Dreigitter-CFD-Laeufe.

## Harte Grenzen

Diese Bridge ist **nur eine diagnostische Kopplung**: die binäre
Hindernismaske und die aus Zellmitten extrapolierte Druckkraft
beeinflussen weder den MAC-Cut-Cell-Projektor noch den bewegten
Festkoerper. Das monotone Spaltenmodell ist bei brechenden Wellen
gerade **nicht** universell anwendbar; reale freie Grenzflaechen
koennen mehrwertig sein. Kein aufgeloester realer P1-P8-Transducer,
keine validierte physikalische Oberflaechenspannung, keine globale
Impuls-/Energiebilanz und keine dynamische bidirektionale FSI.

Der historische gehashte MARIN/SPHERIC-Test-02-Vergleich ueber
**6 s auf drei Gittern bleibt FAIL**: feine H1-H4-RMSE
0.094788 / 0.100674 / 0.117963 / 0.137818 m, gefordert
unveraendert je <=0.05 m. P1-P8 etwa 5.8-5.9 kPa RMSE.
Dieser neue Teilnachweis aendert keine der Labormetriken und
keinen Phase-Gate-Status. **P2=PARTIAL, ALL_COMPLETE=False.**
