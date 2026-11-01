# P2 Teilabnahme: echte 3D-MARIN-Dammbruchmessung auf zwei Gittern

Stand: 01.11.2026. **P2 NICHT voll abgenommen.** Dies ist ein nachvollziehbar
begrenzter Vergleich der vorhandenen CPU-Fluidimplementation mit unabhängigen
Labor-Messwerten. Keine CFD/FSI-Vollvalidierung.

## Tatsächlicher Versuch und Auflösung

Quelle: SPHERIC/ERCOFTAC Test 02, MARIN/Kleefsman, offizielle Messdatei
`test_case_2_exp_data.xls` mit 7.395 Zeitpunkten, H1–H4 und P1–P8.
Original-ZIP und XLS sind mit SHA-256 geprüft und liegen nur lokal im
bestehenden Unterordner `references/`.

Reale C++17/Win64-Implementierung: `CoupledMacWater3D`, Schwerkraft,
MAC-Druckprojektion, konservativer optionaler 3D-Donor-Cell-Durchfluss.
Die genau gleichen Modellparameter und Anfangsbedingungen wurden auf
zwei unterschiedlichen 3D-Rastern berechnet:

| | Grob | Fein |
|---|---|---|
| Zellweite | 0,10 m | 0,05 m |
| Zellen | 32 × 10 × 10 | 64 × 20 × 20 |
| Physikalische Modellgröße | 3,20 × 1,00 × 1,00 m | 3,20 × 1,00 × 1,00 m |
| Zeitintegration | 200 × 0,01 s | 200 × 0,01 s |
| Wasservolumen initial | ca. 0,6644 m³ | ca. 0,6644 m³ |
| Maximaler numerischer Volumendrift | 6,79 × 10⁻¹³ m³ | 2,72 × 10⁻¹² m³ |

Das Modell verwendet weiter ein binäres Hindernis statt exakter
Oberflächengeometrie, die Tanklänge ist um 2 cm verkürzt.
Die Höhe wird durch vertikale Summation des VOF-Füllgrades geschätzt,
nicht durch die reale Messsonde mit Fehler-/Spritzmodellen.
Die Wasser/Luft-Stressbedingungen sind weiterhin ein Näherungsmodell.

## Referenzvergleich: 201 Zeitwerte, keine selektiven Stichproben

Für die Fehlerauswertung werden pro Gitter 200 simulierte Zeitwerte
von 0,01 bis 2,00 s verwendet (zusätzlich t=0), jeweils mit linear
interpolierten echten Experimentaldaten.

| Experimentkanal | Grob RMSE (m) | Fein RMSE (m) | Vorab-Grenze (m) |
|---|---:|---:|---:|
| H1 | 0,120670 | 0,103996 | 0,050 |
| H2 | 0,116936 | 0,076746 | 0,050 |
| H3 | 0,162034 | 0,139819 | 0,050 |
| H4 | 0,205611 | 0,158084 | 0,050 |

**Alle vier Höhen-RMSE sinken bei räumlicher Verfeinerung**, aber
**alle vier verfehlen weiterhin das vorab festgelegte 0,05-m-Kriterium**.
Ein Zweigittervergleich alleine beweist zudem keine asymptotische
Konvergenzordnung oder numerische Unabhängigkeit.

Ein früheres, positiv wirkendes Vergleichsprotokoll benutzte nur
wenige ausgewählte Beobachtungszeitpunkte und meldete beispielsweise
für H2 bei der feinen Variante 0,04469 m. Das ist **nicht** der RMSE der
vollständigen zweisekündigen Zeitreihe: dieser beträgt 0,07675 m.
Diese Abweichung ist dokumentiert, nicht nachträglich wegkalibriert.

## Technische Nachweise und Abnahmegrenzen

- `benchmarks/p2_marin_dambreak_heights.cpp`:
  zwei Auflösungen, native Rücklesung jeder Zeitstufe, MSVC/Release.
  Der ursprüngliche Standardaufruf und das Legacy-Modell bleiben erhalten.
- `scripts/p2_spheric_marin_grid_refinement.py`:
  prüft Quellgeometrie, Gitterkennungen, volle 201 Werte, native
  Exit-PASS-Meldung, Massenerhaltung, RMSE und Originalmessdaten.
- `tests/p2_spheric_marin_grid_refinement_regression.py`:
  Positivprüfung plus negative Manipulationen an Gitterdimensionen,
  fehlenden Zeitwerten, Zeitmonotonie und falscher PASS-Zeile.
- `scripts/p2_spheric_marin_grid_partial_acceptance.py`:
  prüft Teilabnahme unabhängig, erstellt JSON mit Dateihashes und
  erzwingt `phase_complete=false`. Option `--rebuild-and-rerun` baut
  die native EXE neu und wiederholt beide 200-Schritt-Berechnungen.

Nachweise: bestehender Ordner `ENGINE TEST STUBE/runs/`, insbesondere
`p2_marin_throughflow_coarse200_dense.txt`,
`p2_marin_throughflow_fine200_dense.txt`,
`p2_marin_grid_refinement_partial.json` und
`p2_marin_grid_refinement_partial_acceptance.json`.

## Gesperrte P2-Vollphase

Eine echte vollständige P2-Abnahme verlangt weiterhin die unabhängige
experimentelle Übereinstimmung, 6-s-Versuch mit Drucksensoren P1–P8,
mindestens drei Gitter für Gitterordnung, spannungskonsistente
Viskositätsbedingungen an beweglicher Wasser/Luft-Grenze,
dynamische Zweiwege-FSI und physikalische globale Impuls-/Energiebilanzen.
**Keiner dieser offenen Punkte wird durch diesen Teilnachweis als
vollständig erledigt markiert.** `ALL_COMPLETE=False`.
