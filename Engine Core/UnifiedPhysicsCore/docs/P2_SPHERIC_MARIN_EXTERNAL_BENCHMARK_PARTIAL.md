# P2 Teilmeilenstein — offizieller SPHERIC/MARIN Test 02 (01.11.2026)

**Abnahme ausschließlich dieses begrenzten Teilmeilensteins: PASS. Vollphase P2: OFFEN.**
Dieser Lauf ist der erste konkrete Vergleich des vorhandenen nativen 3D-MAC/VOF-Wassermodells
mit einer externen, echten Labor-Messreihe, **kein** erfolgreich validierter 3D-CFD-Solver.

## Quellen und Geometrie

- Offizieller Versuchsdatensatz und PDF: https://www.spheric-sph.org/tests/test-02
  (SPHERIC/ERCOFTAC Test-case 2, Release 1.1, März 2006; Experiment MARIN/Kleefsman).
- Unabhängige Geometriebeschreibung mit identischen Messpositionen:
  https://chaos-polymtl.github.io/lethe/documentation/examples/multiphysics/3d-dam-break/3d-dam-break.html
- Messdaten aus dem originalen ZIP, Workbook `test_case_2_exp_data.xls`.
  Archiv SHA-256: `a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1`.
  XLS SHA-256: `8126f8c8f6effb9e8bfb245a2d098186e9ebee2ef6e75fcb1b3166af17883d8c`.
- Messreihe: **7.395 Zeitzeilen**, **12 Kanäle**: 8 Drucksensoren P1–P8 (Pa),
  4 Höhen H1–H4 (m); Zeitspanne bis 7,394 s.
- Referenzbecken: **3,22 × 1,00 × 1,00 m**, anfänglich Wasser im rechten
  1,228-m-Abschnitt (`x >= 1.992 m`), Höhe 0,55 m, Dichte 1000 kg/m³.
  Hindernis: `x = 0.6635..0.8245`, `y = 0.295..0.698`,
  `z = 0..0.161 m`. Höhenmessstellen: x=0,496 / 0,992 / 1,488 / 2,632 m.
- **Nicht in Git eingecheckt**: Original-ZIP, entpackte Drittdateien,
  1,49-MB-CSV, lokales `xlrd`-Paket. SHA-Fingerprints und Reproduktionsskripte
  sind eingecheckt; Dateien liegen lokal unter `references/`.

## Wirklich durchgeführter nativer Lauf

- `benchmarks/p2_marin_dambreak_heights.cpp` wurde mit MSVC C++17 Release
  gebaut und als echte Win64-EXE mit **200 Schritten × 0,01 s = 2,00 s**
  ausgeführt. Kein Stub, kein nachgebauter Ergebnistext.
- Bestehende `CoupledMacWater3D`-API: Schwerkraft, MAC-Projektion,
  VOF-Wassertransport, feste Hinderniszellen. Raster **32 × 10 × 10**,
  dx=0,10 m; tatsächliche numerische Länge nur **3,20 statt 3,22 m**.
  Hindernis wird binär an Zellzentren approximiert. Wasser/Luft ist kein
  spannungskonsistentes Zweiphasenmodell.
- Anfängliches numerisches Wasservolumen **0,6644 m³** statt
  experimentell geometrisch 1,228×0,55×1,0 = **0,6754 m³**
  (Abweichung wegen abgeschnittenem x-Ende).
- Maximale Drift des simulierten Wasservolumens über die protokollierten
  Zustände: **1,89×10⁻¹⁵ m³**. Das belegt interne Volumenkonservierung,
  **nicht** physikalische Korrektheit der Strömung.
- Höhen werden aus der vertikalen VOF-Zellsumme am mittleren y-Schnitt
  berechnet; das ist bei Spritzern und getrennten Wasserschichten
  nur eine Näherung für die gemessene freie Oberfläche.
- P1–P8 werden nicht simuliert; ein Druckfeld des MAC-Projektors ist
  **kein** Ersatz für reale, kalibrierte Drucksensorzeitreihen.

## Messvergleich — explizit FAIL für physikalische Validierung

Vor dem Vergleich festgelegte quantitative Teilanforderung:
**RMSE <= 0,05 m für jede der vier Höhenmessstellen**;
für die spätere vollständige Abnahme zusätzlich Druckzeitreihen,
mindestens 6 s und Netzverfeinerung.

| Kanal | RMSE (m), gemessen gegen UPC | Limit | Ergebnis |
|---|---:|---:|---|
| H1 | 0,090425 | 0,050 | FAIL |
| H2 | 0,100170 | 0,050 | FAIL |
| H3 | 0,107548 | 0,050 | FAIL |
| H4 | 0,206725 | 0,050 | FAIL |

Die Messreihe wurde mit SHA-Prüfung, streng monotoner Zeit,
Einheiten-/Kanalvalidierung und linearer Zeitinterpolation verarbeitet.
Die numerische Ausgabe wurde auf Native-PASS, Geometrie, Masse und
monotone Zeit geprüft. **9 echte Python-Regressionstests** umfassen
auch Manipulationen am ZIP, Mess-CSV und der Simulationsgeometrie:
alle Manipulationen werden erkannt und abgelehnt.

## Reproduktion auf Windows

Vom Ordner `Engine Core/UnifiedPhysicsCore`:

1. Original-ZIP von der offiziellen SPHERIC-Seite nach
   `references/SPHERIC_Test02_original.zip` herunterladen.
   Die Skripte prüfen den festgelegten SHA-256 und brechen bei Abweichung ab.
2. Einmalig `python -m pip install --no-deps --target references/vendor xlrd==2.0.2`
   ausführen (nur lokale Referenzbibliothek; außerhalb Git).
3. `python scripts/p2_spheric_marin_partial_acceptance.py` starten.
   Es baut mit VS 2026/MSVC BuildTools, führt die native EXE und die
   Daten-/Vergleichs-/Negativtests aus und schreibt einen **Teilreport**.

Nachweise liegen unter `ENGINE TEST STUBE/runs/`:
`p2_spheric_marin_measured_ingestion.json`,
`p2_marin_baseline_200steps.txt`,
`p2_spheric_marin_cfd_comparison_fail.json`,
`p2_spheric_marin_regressions.txt`,
`p2_spheric_marin_partial_acceptance.json`.
Jeder Teilreport enthält SHA-256-Hashes der verwendeten Dateien und
weist `phase_complete=false` aus.

## Nicht erfüllte P2-Vollabnahme

Die reale Messvalidierung **ist gescheitert** (alle vier RMSE über Limit).
Außerdem fehlen der volle >=6-s-Lauf, experimenteller Druckvergleich P1–P8,
Rasterverfeinerung und unabhängige Gitterkonvergenz, korrekte
Wasser-Luft-Grenzflächenspannungen und dynamische Zweiwege-FSI samt
physikalischer Energie-/Impulserhaltung. Kein vollständiger P2-PASS,
kein P3–P8-Vollphasenabschluss und kein neuer Unreal-5.8-Native-E2E.
