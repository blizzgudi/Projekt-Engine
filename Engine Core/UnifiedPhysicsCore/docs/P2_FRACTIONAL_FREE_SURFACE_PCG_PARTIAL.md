# P2: geometrischer Wasser-Luft-Abstand im echten 3D-MAC/PCG (Teilnachweis)

Stand: 01.11.2026. **P2 weiterhin PARTIAL, keine volle CFD-/FSI-Abnahme.**

## Technische Änderung

Der bisherige opt-in PCG-Druckprojektor in include/upc/mac3d.hpp verwendet an Wasser-Luft-Flächen standardmäßig den Halbzell-Ghost 2/dx. Neu können die drei staggered-Face-Richtungen **x/y/z** einzeln einen geometrisch vorgegebenen Abstand theta * dx zwischen nassem Zellzentrum und dem Ort p_air=0 erhalten (0 < theta <= 1). Die zugehörige Poisson-Diagonale und der nachfolgende MAC-Geschwindigkeitsgradient verwenden konsistent 1/theta statt pauschal 2. Feste Wände und Wasser-Wasser-Flächen werden nicht umgewichtet. Ungültige Koordinaten, NaN/Inf und nichtpositive Distanzen werden abgewiesen.

Die Distanzarrays werden **nur bei expliziter Nutzung** angelegt. Der Default-PCG und die alte Gauss-Seidel-Projektion behalten ihren Halbzellwert und den ursprünglichen numerischen Pfad. Es handelt sich um eine geometrische Dirichlet-Druckrandbedingung, **nicht** um spannungskonsistente Zweiphasenphysik, Luftdrucklösung, Oberflächenspannung, VOF-/PLIC-Oberflächenrekonstruktion oder ein vollständiges FSI-Modell. Die MARIN-Live-Simulation verwendet weiterhin den bisherigen Halbzellpfad; es wurde **keine** ungesicherte Umdeutung von VOF-Füllgraden in Abstände vorgenommen.

## Echte native Nachweise

Der neue Test tests/p2_fractional_free_surface_pcg_tests.cpp löst hergestellte 3D-Schichten auf **5³ und 9³**, jeweils in x/y/z, mit drei vorgegebenen Abständen (0,25 / 0,5 / 0,75) und spiegelbildlich angeordneter Luftgrenze. Insgesamt **21 Fälle** und **3.209 native Assertions PASS**. Die unabhängig hergeleitete Impulsreferenz bei rho=1000 kg/m³, dx=1 m, dt=0,1 s und u=1 m/s lautet p_wet = -rho * dx/dt * theta und ergibt -2500/-5000/-7500 Pa. Der Druck wird auf beiden nassen Zelllagen und die tatsächlich projizierte Face-Geschwindigkeit auf Null geprüft. Default und explizites theta=0,5 liefern bitgenau denselben Wert. Falsche Abstände/Indizes werden abgewiesen.

MSVC Release / Ninja wurde nativ gebaut. **120/120 CTest PASS** (75,61 s). Die Python-QA prüft unabhängig die 21 numerischen Messwerte, Fallabdeckung, Assertion-Zahl, vollständige CTest-Suite, Quell-/Binär-/Log-Hashes und den **negativen** echten Phase-Gate-Status. Vier mutierte Logs (Druck verfälscht, Achse entfernt, falscher Vollabschluss, NaN) wurden tatsächlich zurückgewiesen.

**Negative Mess-Evidenz bleibt bestehen:** Die vorherige MARIN-Wandnormal-Analyse meldet P1–P8-Druck-RMSE von ca. 4,3 bis 5,0 kPa und FAIL gegenüber echten Sensoren. Der neue geometrische Test korrigiert diese Messabweichung nicht. Es erfolgte kein neuer teurer 6-s-Lauf.

## Artefakte und SHA256

Alle Pfade liegen ausschließlich unter D:\Projekt Engine.

| Datei | SHA256 |
|---|---|
| Engine Core/UnifiedPhysicsCore/include/upc/mac3d.hpp | 4d206fa8f63827f1035ce6317c636df479abfae2a2f05e9b84fdf5c3160e5853 |
| Engine Core/UnifiedPhysicsCore/tests/p2_fractional_free_surface_pcg_tests.cpp | 9c7273b464e0dfd187b2e1eef5ba9bae5efe306f48d953b971d4b4842230e675 |
| Engine Core/UnifiedPhysicsCore/build/upc_p2_fractional_free_surface_pcg_tests.exe | 24241a8490d8c0f575f702f8361eb9c283ddccbbd3475af4abc1c65d9d323598 |
| ENGINE TEST STUBE/runs/p2_fractional_surface_native.txt | 6fc6155edd6752ba2e63f5cab846193f02a61322b324e9c7d3c6e1caa0e3487b |
| ENGINE TEST STUBE/runs/p2_fractional_surface_ctest.txt | 4cdee1c5ad547d990f0d35a7017b6666940f37ffe8d004502c3aabd7df573bec |
| ENGINE TEST STUBE/runs/p2_fractional_surface_partial.json | 3dcb57d80acc4843fac4e3d73ab67c9c256cb282c1762aa51b3d46bc1e6c5a69 |
| ENGINE TEST STUBE/runs/p2_fractional_surface_phase_gate.txt | 81c1aeb5f80e7e0c8ab439c351b9eab256694a788538ae1ad32d0f2082f8cccf |
| ENGINE TEST STUBE/runs/p2_fractional_surface_mutations.txt | 4e312421ce07d6578bc5f24f64d941d22ffcef35938d96e9206ac37f70c579d8 |

Reproduktion: scripts\p2_fractional_free_surface_pcg_build.cmd --run baut und testet nativ; ohne Argument werden nur vorhandene Belege geprüft. Das vollständige --run wurde mit ExitCode 0 ausgeführt. Die Python-Teilabnahme ist scripts/p2_fractional_free_surface_pcg_partial_acceptance.py.

## Verbindlicher Status und nächste P2-Arbeit

Das reale scripts/phase_gate.py meldete: GATES_PASS=P0, GATES_INCOMPLETE=P1,P2,P3,P4,P5,P6,P7,P8, ALL_COMPLETE=False. Die historische P1-Evidenz ist wegen eines alten CMake-Quellhashs nicht mehr aktuell; dies wurde nicht durch manuelles Umschreiben von Prüfergebnissen verdeckt.

Nächster notwendiger Schritt: robuste geometrische 3D-VOF-/PLIC-Oberflächenrekonstruktion für die echten MAC-Faces mit gemessener Gitterkonvergenz und anschließend eine tatsächlich spannungskonsistente Wasser-Luft-Kopplung. Zusätzlich fehlen messkonvergente H1–H4/P1–P8 über mindestens sechs Sekunden auf drei unabhängigen Gittern, bewegte bidirektionale FSI und globale physikalische Impuls-/Energie-/Massenbilanzen. **P2 ist nicht PASS.**
