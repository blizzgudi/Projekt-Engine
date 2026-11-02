# P2-Teilnachweis: 3D-Wasser/Luft-Gitter mit konservativem Scherimpulsaustausch (02.11.2026)

## Implementierter Umfang

`include/upc/p2_two_phase_grid_shear3d.hpp` erweitert den vorher isolierten Zwei-Zellen-Couette-Austausch auf ein **räumliches 3D-Gitter** mit getrennten Wasser- und Luftgeschwindigkeiten. Jede scharfe Wasser/Luft-Zellnachbarschaft in x-, y- oder z-Richtung wird genau einmal verarbeitet. Die Normalrichtung folgt immer von Wasser nach Luft. Die beiden Zellmassen ergeben sich aus ihren Dichten und `dx^3`; die Grenzfläche aus `dx^2`, die Abstände aus `dx/2`. Beide Zellgeschwindigkeiten werden durch einen impliziten tangentialen Impulsaustausch tatsächlich aktualisiert. Ein optionaler einheitlicher Marangoni-Gradient muss an **allen** betroffenen Flächen tangential sein.

Bei mehrfach angrenzenden Flächen werden die Impulse in deterministischer Reihenfolge sequenziell auf den jeweils aktuellen Zellgeschwindigkeiten verarbeitet. Globale Wasser-/Luftmassen, Gesamtimpuls vor/nach dem Schritt, explizite Oberflächenimpulsquelle, kinetische Energie, Oberflächenarbeit, physikalischer viskoser Verlust und zusätzliche implizite Backward-Euler-Dissipation werden getrennt bilanziert. Eine Kopie der Geschwindigkeiten garantiert, dass fehlerhafte Eingaben oder eine ungültige Grenzfläche den Zustand nicht teilweise verändern.

**Wichtig:** Das Gitter ist **zellzentriert**, hat eine **feste scharfe 0/1-Phasenmaske** und stellt noch keine Integration in die staggered MAC/VOF-/PLIC-Advektion oder die PCG-Druckprojektion dar. Die Normalgeschwindigkeiten werden je Kontakt nicht verändert, aber andere Kontaktflächen derselben Zelle können andere Komponenten ändern. Daher ist das **keine** vollständige Mehrphasen-No-Penetration- oder stresskonsistente Randbedingung. Es gibt keine bewegten Festkörper, keine Luftkompressibilität, keine Kapillarenergie und keine dynamische Oberflächengeometrie.

## Native Tests und unabhängige Referenz

`tests/p2_two_phase_grid_shear3d_tests.cpp` prüft ein planares 4x4x4-Gitter mit 16 x-Grenzflächen und die analytische implizite Couette-Referenz (rho_w=1000, rho_a=1.2 kg/m³, mu_w=1, mu_a=0.1 Pa*s, dx=0.1 m, dt=0.05 s). Der Widerstand ist `R=0.05/1+0.05/0.1=0.55`, `b=dt*dx²/R` und `D_new=1/(1+b*(1/m_w+1/m_a))`. Der tatsächlich gemessene Slip ist `0.5686713768740307 m/s`, der Wasserimpuls pro Face `0.0005169739789763915 N*s`. Wasser: 32 kg, Luft: 0.0384 kg. Zusätzlich: 16-facher Marangoni-Quellimpuls und globale Energie-/Impulsbilanz.

Ein nichttriviales 3x3x3-Schachbrett mit 54 Interfaces (18 pro Achse), verschiedenen 3D-Geschwindigkeiten und 2 zeitlichen Schritten prüft die sequenzielle Impuls- und Energiebilanz, deterministische Wiederholung und Galilei-Invarianz. Ungültige Größen, Phasen, NaN, Inf und ein normaler Marangoni-Gradient werden abgewiesen, ohne die Geschwindigkeiten zu verändern.

Der erste echte MSVC-Release-Build und die vollständige **133/133 CTest-Suite PASS**. Der neue native Test meldete **325 Assertions PASS**, maximales 3D-Impulsresiduum `4.440892098500626e-16 N*s`, Energie-Residuum `-2.044144611608803e-17 J` im Schachbrett. Die separaten Python-QA- und Hashnachweise stehen unter `ENGINE TEST STUBE/runs/p2_two_phase_grid_shear_partial.json`. Sie vergleichen die analytische Referenz unabhängig, führen die native Binary erneut aus, verwerfen manipulierte Logs und prüfen `phase_gate.py`.

## Negativer experimenteller Status und offene Vollkriterien

Der bisherige **echte 6-s-MARIN/SPHERIC-Vergleich bleibt FAIL**: H1-H4 überschreiten die festgelegten 0,05-m-RMSE-Grenzen. Keine neue >=6-s-Dreigitter-Messkonvergenz, kein kalibrierter P1-P8-Transducer-Abgleich, keine dynamische vollständige Wasser/Luft-Spannungsrandbedingung, keine bidirektionale Fluid-Festkörper-Kopplung, keine physikalische globale Gesamtbilanz einschließlich Advektion/Druck/Gravitation/Oberflächenenergie und kein Unreal-5.8-Native-E2E für diese Erweiterung.

**P2 bleibt PARTIAL; phase_complete=false; ALL_COMPLETE=False.** Der lokale Gitter-Scheroperator ist ein echter begrenzter Teilbaustein, kein vollständiger 3D-CFD- oder P2-Vollabnahmenachweis.
