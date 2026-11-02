# P2 – unabhängige sechssekündige MARIN/SPHERIC-Gitterprüfung

**Stand: 02.11.2026. Mess-/Numerik-Teilnachweis; keine P2-Vollabnahme.**

## Motivation und reproduzierbare Ausführung

Bisher wurde ein echter MARIN/SPHERIC-Test-02-Wasser-/Druckvergleich
über das vollständige 6-Sekunden-Fenster nur auf einem groben
3D-Gitter ausgeführt. Vergleiche auf 0,05 m Auflösung umfassten
nur die ersten 2 Sekunden. Das erlaubte keinen echten Vergleich
des vollständigen Messverlaufs mit räumlicher Verfeinerung.

Ergänzt wird hier eine reproduzierbare, vom selben aktuellen
MSVC-Benchmark erzeugte Zweigitter-Abnahme. Die native
`benchmarks/p2_marin_dambreak_heights.cpp` wurde aus dem
tatsächlich aktuell vorhandenen Source-Bestand neu kompiliert.
Ein älteres, bereits gestartetes 6-Sekunden-Binary wurde als
**veraltet erkannt, angehalten und dessen unvollständige Daten
getrennt als nicht abnahmefähig archiviert**, statt es als
vergleichbare Messung zu deklarieren.

Die aktuelle Binärdatei wird nach beiden vollständigen
Native-Läufen per SHA256 mit den erzeugten Reports verbunden.

Beide Versuchsläufe nutzen:
- Offizielles original Test-02-MARIN-CSV aus der unverändert
  gehashten XLS-Versuchsreihe mit 7.395 Zeitpunkten und
  12 realen Druck-/Höhensensor-Kanälen, archiviert unter
  `references`.
- Identische Versuchsdauer 6,0 s, 600 Zeitschritte zu 10 ms,
  601 tatsächliche Zustände pro Gitter.
- Identischen opt-in konservativen Wassertransport,
  variable 3D-MAC-PCG-Projektion und tangential interpolierte
  Druckauslesung mit exakt denselben acht Sensor-Koordinaten.
- Grobes räumliches Gitter 32×10×10 bei dx=0,1 m.
- Feines räumliches Gitter 64×20×20 bei dx=0,05 m.
- Annähernde Tank-Domäne 3,20×1×1 m, Anfangswasserreservoir
  x=1,992…3,20 m bis Höhe 0,55 m, stationäres Hindernis
  x=[0,6635,0,8245], y=[0,295,0,698], z=[0,0,161] m.
- Geschlossene Volumenbilanz mit maximal 1e-8 m³ Fehler
  und projizierte 3D-Divergenz von maximal 1e-6 s^-1.

## Unabhängige Auswertung und Negativkontrollen

Die neue Auswertung `scripts/p2_marin_full6s_two_grid_audit.py`
akzeptiert ausschließlich tatsächlich abgeschlossene 601er
Zustandsreihen mit korrektem Native-Endmarker.
Sie überprüft jede Probe und jeden Zeitstempel, den genauen
Drucklöser-Modus, das Ausgangsvolumen, tatsächliche
PCG-Divergenz sowie die unveränderten Koordinaten des
versuchsnahen Hindernisses.

Die P1–P8-Fehler sind Messdifferenzen in Pascal,
die H1–H4-Fehler in Metern. Alle Zeitreihen werden erst
nach separater Hash-Prüfung der originalen Experimentdaten
zeitlich interpoliert. Der **vorher festgelegte**
Höhen-RMSE-Grenzwert von 0,05 m bleibt unverändert.
Darüber hinaus wird protokolliert, ob sich die individuellen
Kanäle unter Gitterverfeinerung wirklich verbessern.

`tests/p2_marin_full6s_two_grid_regression.py` verifiziert
pro Lauf die negativen Kontrollbeispiele: gekürzte Zeitreihen,
falsche Projektionsmethode, veränderte Hinderniskoordinaten,
falsches Gitter, manipulierte Druckreferenz,
NaN/Infinity, verlorene Massenerhaltung, beschädigte
Zeitschritte, fehlenden Endmarker und gefälschte P2-PASS-
Aussagen. Die Rohdaten und das maschinenlesbare Ergebnis
liegen ausschließlich im bestehenden Ordner
`ENGINE TEST STUBE/runs`.

## Noch offene echte physikalische P2-Vollabnahme

**Die vollständige sechssekündige Messung auf zwei Gittern
ist keine konvergierte, experimentell valide 3D-CFD.**
Für einen vollständigen PASS fehlen selbst bei bestandenen
numerischen Checks weiterhin:
- H1–H4: RMSE <=0,05 m im originalen 6-s-Datensatz
  auf dem konvergierten feinsten Gitter.
- P1–P8: vom Labor nachweisbar aufgelöste Druck-Sensorfläche,
  richtiger Wandimpuls und quantitativ nachgewiesene
  Messübereinstimmung (kein Druck-Proxypass).
- Mindestens ein drittes unabhängiges 3D-Gitter für
  räumliche Konvergenz, ohne Schwellenwerte nachträglich
  an bereits beobachtete Fehler anzupassen.
- Scharfe bewegte Wasser-Luft-Grenzflächenspannung,
  global korrekt geschlossene physikalische Energie
  und Impulsbilanz sowie dynamische bidirektionale
  Fluid-Festkörper-Kopplung (FSI).
- Verifizierbarer echter aktueller Native-E2E der
  gesamten gekoppelt simulierten Physikkette.

**Offizieller Status: P2 = PARTIAL, phase_complete=false.**
