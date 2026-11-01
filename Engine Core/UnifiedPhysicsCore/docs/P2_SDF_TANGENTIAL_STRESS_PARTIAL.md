# P2-Teilnachweis: tangentiale 3D-Wasser-Luft-Spannung am SDF-Schnitt (01.11.2026)

## Tatsächlich implementierter Teil

Neuer optionaler, isolierter Baustein `include/upc/p2_sdf_tangential_stress3d.hpp` ergänzt die bestehende 3D-SDF/Young-Laplace-Normalspannung um die **tangentiale viskose Spannungsbedingung** mit vorgegebenem Oberflächenspannungsgradienten. Für jede echte 3D-MAC-Schnittfläche mit SDF-Normale `n` (Wasser nach Luft) wird die tangentiale Traktion

`t = (I - n*n^T) * [mu_w*(G_w+G_w^T) - mu_a*(G_a+G_a^T)] * n`

mit dem Marangoni-Ziel `(I-n*n^T) * grad(sigma)` verglichen. Ein **lokaler, symmetrischer Korrekturgradient** auf Wasserseite wird mit `Delta G = (r*n^T+n*r^T)/(2*mu_w)` konstruiert, wobei `r=target-t` tangential ist. Damit wird die tangentiale Traktion algebraisch an die Randbedingung angepasst, ohne den bereits validierten normalen viskosen Drucksprung `2*mu*n.D.n` zu ändern. Bei Nullviskosität und nicht erfüllbarer tangentialer Traktion wird abgelehnt; nichtfinite Daten und ungültige SDF werden ebenfalls abgewiesen.

**Wesentliche Abgrenzung:** Dies ist eine lokale Rekonstruktion eines vorgegebenen Rand-Geschwindigkeitsgradienten, **keine** zeitlich fortgeschriebene Luft-/Wasserströmung. Der Korrekturgradient wird **nicht** in die MAC-Face-Geschwindigkeiten, VOF-Advektion oder Druckprojektion zurückgespeist. Die Oberflächenspannungsgradienten werden extern vorgegeben. Es ist weder ein dynamischer Marangoni-Solver noch eine voll stresskonsistente freie Oberfläche.

## Native Windows-Messung und unabhängige Referenz

Reproduktion: `scripts/p2_sdf_tangential_stress_build.cmd`, danach `python scripts/p2_sdf_tangential_stress_acceptance.py`. Die MSVC-Release-Tests verwenden 12³ und 24³ echte 3D-SDF-Gitter, nicht 2D-Extrusionen für den Kugelfall.

- Planare x-Grenze, 144 geschnittene Faces, `mu_w=2 Pa*s`, `mu_a=0,5 Pa*s`, vorgeschrieben `grad(sigma)=(0,11;0,6;-0,25) N/m²`. Unabhängige Tensorauswertung liefert vor Korrektur `t_y=0,555 Pa`, `t_z=0,89 Pa` und Residuum `sqrt(0,045²+(-1,14)²)=1,14089 Pa`. Nach Korrektur: maximales Residuum **1,11022e-16 Pa**, Änderung des normalen Spannungssprungs **0 Pa**.
- Kugel `R=0,287 m`, `dx=0,08 m`: 264 3D-Schnittfaces, maximales Tangentialresiduum **5,08768e-16 Pa**, maximale Änderung des normalen Sprungs **1,11022e-15 Pa**.
- Kugel `dx=0,04 m`: 984 Schnittfaces, maximales Tangentialresiduum **7,36439e-16 Pa**, maximale Änderung des normalen Sprungs **1,33227e-15 Pa**.
- Der gespeicherte Normaldruck ist in beiden Kugelgittern exakt unverändert (maximaler Druckunterschied 0 Pa). Der native Test enthält **10.188 Assertions**, darunter unabhängige Tensorformeln, Normal-/Tangentialtrennung, 5 absichtlich ungültige Eingaben und den gültigen Nullviskositäts-Nullschub-Fall (144 Faces).
- Die vollständige native MSVC-Release-CTest-Suite hat **130/130 PASS** in **34,48 s** erreicht. Nach Änderung der CMake-Datei wurde die P1-Referenzabnahme tatsächlich erneut ausgeführt (33/33 quantitative Checks PASS), sodass das offizielle Gate wieder **P0,P1 PASS; P2–P8 INCOMPLETE** ausweist. Das Ergebnis wird ausschließlich aus dem tatsächlich gespeicherten CTest-Log übernommen. Das QA-Skript vergleicht einen unabhängigen erneuten Native-Lauf, prüft sechs manipulierte Protokolle und speichert SHA256 für Quellen, Binary, Logs und historische MARIN-Messung.

## Negative Ergebnisse und noch fehlende Vollabnahme

Die bereits vorhandene reale **6,00-s-MARIN/SPHERIC-H1-H4-Messvalidierung bleibt FAIL** (siehe `ENGINE TEST STUBE/runs/p2_marin_pcg_6s_partial.json`). Dieser Schritt führt bewusst keinen neuen teuren Dreigitterlauf aus. Weiter offen: laufende VOF/SDF-Krümmung, dynamische Luftgeschwindigkeit, gekoppelte tangentiale MAC-Ghost- und Normalspannungsprojektion, vollständiger 6-s-H1-H4/P1-P8-Messabgleich mit unabhängiger Gitterkonvergenz, bidirektionale bewegte Festkörper und global geschlossene physikalische Massen-/Impuls-/Energiebilanzen sowie neues UE5.8-Native-E2E.

Erster unabhängiger QA-Lauf **FAIL**: Der Test zur Erkennung manipulierter Traces veränderte nur den Präfix einer sehr kleinen Exponentialzahl (beispielsweise `1.1e-16` zu `91.1e-16`); das lag weiterhin unter der zulässigen Toleranz. Die QA-Manipulation wurde korrigiert, indem der gesamte Zahlenwert gezielt durch `9` ersetzt wird. Danach wurden **6/6 gefälschte Traces abgelehnt**, ohne die numerischen Physik-Toleranzen zu lockern.

Die synthetischen Randtests belegen nur die lokale algebraische Konsistenz, **nicht** die Gesamtphysik. Die Phase bleibt im offiziellen `scripts/phase_gate.py` **P2 PARTIAL / ALL_COMPLETE=False**. Die QA erzeugt `ENGINE TEST STUBE/runs/p2_sdf_tangential_stress_partial.json` mit `phase_complete=false`.
