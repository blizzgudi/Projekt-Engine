# P2 – MARIN Test 02: raeumlich variable Wasserlinie an der Quaderwand (Teilnachweis)

**2026-11-02. P2 bleibt PARTIAL, keine Labor-/Native-E2E-Vollabnahme.**

## Reproduzierter Defekt und eng begrenzte Korrektur

Der bisherige `P2MarinBoxWallTraction3D::audit_wetted` kann ausschliesslich eine global horizontale Wasserlinie verarbeiten. Bei 3D-Dammbruchwellen unterscheiden sich benetzte Flaechen auf der Anstroem- und Abstroemseite sowie auf benachbarten Quaderstreifen. Der neue optionale Aufruf `audit_heightfield(..., waterline_xy_m)` akzeptiert je horizontaler Gitterspalte eine eigene Wasserlinie in Metern. Die benetzte Hoehe wird fuer jede vertikale Wandkachel aus der benachbarten **fluessigkeitsseitigen** Spalte genommen; das Dach wird nur fuer Spalten mit Wasserstand >= Dachhoehe belastet. Geometrisch geschnittene Wandflaechen werden vor der Druckrekonstruktion beschnitten. Ungueltige Hoehen, fehlende Spalten und nichtfinite Werte werden abgewiesen. Alte `audit`- und `audit_wetted`-APIs bleiben unveraendert.

**Bewusst begrenzt:** Das Eingabefeld ist eine extern vorgegebene, horizontal stueckweise konstante Hoehenkarte, keine aus der produktiven 3D-VOF-Stroemung rekonstruierte bewegte Grenzflaeche. Es erfasst keine ueberhaengenden Wasserzungen, Luftblasen, freie Grenzflaechenspannungen, Kontaktwinkel oder dynamische Druckrandbedingungen. Der Operator liefert eine Kraft-/Drehmoment-**Diagnostik**, die weder MAC-Projektion noch Koerperbewegung beeinflusst.

## Native und unabhaengige Referenz

Die neue MSVC-Native-Regression prueft 32x10x10, 48x15x15 und 64x20x20 mit dem unveraenderten MARIN-Test-02-Quader (x=[0.6635,0.8245], y=[0.295,0.698], z=[0,0.161] m). Ein ungleichfoermiges Feld `h(x,y)=0.025+0.020*((x+2*y) mod 9) m` schneidet Seitenflaechen und benetzt nur Teile des Daches. Konstanter, explizit hergestellter Zelldruck von 3000 Pa trennt Geometriefehler von Drucksolverfehlern. Die analytische Referenz integriert unabhaengig jede vertikale Spaltenstreifenflaeche, deren Kraft und deren Hebelarm zum Quaderzentrum; Dachlasten werden auf geometrische x/y-Ueberlappungen begrenzt. Geprueft sind alle drei Kraft- und alle drei Drehmomentkomponenten, Flaeche, uniforme Rueckwaertskompatibilitaet, vollstaendige Trockenheit und vier negative Eingaben.

Der neue Test schlug **vor Implementierung** nachweislich beim MSVC-Build fehl (C2039 fehlendes `audit_heightfield`), protokolliert in `ENGINE TEST STUBE/runs/p2_marin_heightfield_pre_fix_build_20261102.log`. Nach Implementierung: `ENGINE TEST STUBE/runs/p2_marin_heightfield_build_20261102.log`, `p2_marin_heightfield_native_20261102.log`, `p2_marin_heightfield_ctest_20261102.log`; unabhängige Rationalzahl-Python-Referenz, Negativmutationen und SHA256 in `p2_marin_heightfield_wall_partial.json`. Die gesamte native CTest-Suite wurde erneut ausgefuehrt, keine teure 6-s-Dreigitter-CFD-Simulation.

## Offene physikalische Akzeptanz

- Kein 3D-VOF-Interface-Rekonstruktionspfad in der produktiven Simulation; kein spannungskonsistenter Luft/Wasser-Uebergang.
- Keine geometrisch korrekte Cut-Cell-MAC-Projektion im produktiven MARIN-Laborbenchmark; Hindernis dort weiterhin binär.
- Kein kalibrierter, realer P1-P8-Transducer-Durchmesser, keine Labor-Druckimpuls-/Wellenhoehenkonvergenz.
- Keine dynamische bidirektionale FSI, kein globales physikalisches Impuls-/Energiekonto.
- Der gehashte historische echte 6-s-Dreigitter-Vergleich bleibt **FAIL**: H1-H4 am feinsten Gitter 0.094788/0.100674/0.117963/0.137818 m, gefordert jeweils <=0.05 m; P1-P8 ca. 5.8-5.9 kPa RMSE. Keine nachtraegliche Grenzwertveraenderung.

**P2=PARTIAL, ALL_COMPLETE=False.**
