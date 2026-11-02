# P2: echte 3D-Dreigitter-Simulation über das vollständige MARIN-Test-02-Messfenster

**Stand: 02.11.2026. NUMERISCHER TEILNACHWEIS, KEINE P2-VOLLABNAHME.**

## Versuchsaufbau

Bisher bestanden echte externe SPHERIC/MARIN-Test-02-Vergleiche
entweder aus zwei echten Auflösungen über 2 s oder einem groben
3D-Gitter über 6 s. Dieser Meilenstein erweitert die nativen
bereits implementierten Wasser-/MAC-PCG-/VOF-Prototypen um ein
**drittes mittleres Gitter**, ohne die bestehenden Steueroptionen
für grob und fein zu ersetzen.

`benchmarks/p2_marin_dambreak_heights.cpp` behält die
bekannten coarse- und fine-Befehle unverändert und ergänzt
`medium` mit echter räumlicher Dimension 48×15×15
und dx = 3,2 / 48 m. Die physikalisch gleich große,
im Vergleich zum Original um 0,02 m gekürzte Domäne
3,20 × 1 × 1 m bleibt bei allen drei Gittern unverändert.
Die Initialwasserfüllung x=1,992…3,20 m, H=0,55 m
wird durch Volumenüberlappung berechnet, das Hindernis
x=[0,6635;0,8245], y=[0,295;0,698],
z=[0;0,161] m derzeit lediglich durch binäre Zellmitten
aufgelöst. Letzteres ist **keine geometrisch aufgelöste
Strömung am Versuchshindernis**.

Für jede Auflösung ist eine vollständig native Sequenz
aus 600×0,01 s gerechnet worden mit 601 Zuständen,
variabel projiziertem 3D-MAC-Druck, konservativem
Wasservolumentransport und acht druckseitig
tangential interpolierten Sensorwerten:

| Gitter | Zellen | dx | Zeit |
|---|---|---|---|
| coarse | 32 × 10 × 10 | 0,100000 m | 6,0 s |
| medium | 48 × 15 × 15 | 0,066667 m | 6,0 s |
| fine | 64 × 20 × 20 | 0,050000 m | 6,0 s |

**Provenienz und QA:** Alle drei Datensätze entstehen
aus dem *selben* neu gebauten Win64-MSVC-Benchmark.
Die ursprüngliche ZIP-/XLS-/CSV-Reihe mit 7.395
Laborzeitpunkten und zwölf Kanälen wird an SHA256
verifiziert. Es wird eine echte zeitliche Interpolation
auf den Laborkanal verwendet, keine künstliche
Simulation eines Messwertes.

Das neue Skript
`scripts/p2_marin_full6s_three_grid_audit.py`
prüft Native-Endmarker, 600 Zeitintervalle je Gitter,
Wasservolumenfehler unter 1e-8 m³, gemessene
3D-Projektionsdivergenz unter 1e-6 s⁻¹, identische
physikalische Anfangsfüllung und alle zwölf
H1–H4/P1–P8-Messsignale. Die Regression in
`tests/p2_marin_full6s_three_grid_regression.py`
prüft zehn positive/negative Python-Prüfungen und
**12 absichtlich falsche Protokoll-Varianten je Gitter**.

## Aussagekraft und echte noch ausstehende Abnahme

Die vorher festgelegte 0,05-m-RMSE-Schranke für jeden
Wasserstandskanal wird **nicht nachträglich verändert**.
Die räumliche Verbesserung eines einzelnen Kanals
garantiert weder asymptotische Konvergenz noch
experimentelle Übereinstimmung. Die Druckauslesung
bleibt ein geometrischer interpolierter Wanddruck-
Proxy und ist keine kalibrierte Versuchstransducer-
Integration.

Selbst bei drei vollständig fehlerfrei durchlaufenen
Simulationen ist ein **physikalischer P2-PASS**
nicht zulässig ohne:

- belegte Wasserstands-RMSE H1–H4 <=0,05 m;
- tatsächliche 3D-Wand-/Drucksensor-Durchmesser,
  Druckstoß- und Kraftübereinstimmung P1–P8;
- experimentell relevante asymptotische
  Dreigitter-Konvergenz der **physikalischen Messwerte**;
- spannungsrichtige freie Wasser/Luft-Oberflächen
  und geeignete Grenzflächen-/Dissipationsbilanz;
- echte bidirektionale Fluid-Festkörper-Momentum-
  und Energiekopplung sowie dynamische Hindernisse;
- Native-E2E des aktuellen vollständigen Physikpfads.

Die Messergebnisse werden im Report
`ENGINE TEST STUBE/runs/p2_marin_pcg_full6s_three_grid_partial.json`
samt genauen SHA256 von Binärdatei, Source, Labor-CSV
und allen drei Native-Zeitreihen gespeichert.
**P2 bleibt PARTIAL und ALL_COMPLETE=False.**


## Tatsächliche 6-s-Vergleichsergebnisse (native Messungen, kein Modell-Fit)

Die drei **vollständig aus demselben aktuellen C++17/MSVC-Binary
gerechneten** Native-Protokolle enthalten je 601 Zustände.
Die unabhängige Regressionssuite hat **10/10 Tests** und
**36/36 bewusst beschädigte 3D-Messprotokolle** bestanden/abgewiesen.

Wasserhöhen: Quadratischer Mittelwert der Differenz zum echten
experimentellen Laborwert, **RMSE in m**:

| Höhe | 0,100 m (coarse) | 0,066667 m (medium) | 0,050 m (fine) | ≤0,050 m? |
|---|---:|---:|---:|---|
| H1 | 0,136525 | 0,091214 | 0,094788 | **NEIN** |
| H2 | 0,091500 | 0,076981 | 0,100674 | **NEIN** |
| H3 | 0,148245 | 0,124602 | 0,117963 | **NEIN** |
| H4 | 0,224399 | 0,149169 | 0,137818 | **NEIN** |

Alle vier finalen RMSE sind **oberhalb** des zuvor festgesetzten
5-cm-Grenzwerts. Nur H3/H4 fallen monoton bei Verfeinerung;
H1 verschlechtert sich von 0,0667 auf 0,05 m, H2 sogar deutlich.

Drucksensoren: RMSE zum offiziellen Labor, **in Pa**:

| Druck | coarse | medium | fine |
|---|---:|---:|---:|
| P1 | 3.803 | 4.876 | 5.870 |
| P2 | 4.089 | 5.015 | 5.905 |
| P3 | 3.906 | 4.852 | 5.800 |
| P4 | 3.854 | 4.873 | 5.823 |
| P5 | 3.358 | 5.062 | 5.890 |
| P6 | 3.327 | 4.984 | 5.842 |
| P7 | 3.252 | 4.956 | 5.823 |
| P8 | 3.337 | 5.083 | 5.914 |

Diese Angaben verwenden für Deutsch Tausenderpunkte:
beispielsweise 3.803 Pa = ca. 3,8 kPa, nicht 3,803 Pa.
**Alle acht** Druck-Messabweichungen **wachsen**
mit räumlicher Verfeinerung. Dies belegt, dass der
Drucksignal-Proxypfad aktuell keine physikalisch
konvergierte Sensorauslesung ist.

Die Native-Abnahme für 3×6 Sekunden zeigt damit
numerische Ausführbarkeit, native Massenerhaltung
und PCG-Divergenzprojektion, **keine** ausreichende
physikalische CFD-Messgenauigkeit.

Die nächsten technisch nötigen Arbeiten sind
eine korrekt geometrisch aufgelöste Versuchskiste
und die echte Drucktransducer-Fläche, korrekt
bewegte Wasseroberfläche mit konsistentem
Massen-/Impulstransport, sowie bidirektionale
mechanische Wandkopplung; erst dann wieder
identischen 6-s-Dreigitter-Lauf messen.

**Keine retrospektive Grenzwertänderung; P2 ist weiter PARTIAL.**
