# P2 – Live-Achs-VOF-Geometrie in der gekoppelten 3D-MAC-PCG-Projektion (Teilnachweis)

Stand: 01.11.2026. **P2 ist nicht vollständig abgenommen.**

## Implementierter Umfang

Der bestehende analytisch geprüfte Brückencode `p2_axis_vof_pcg_geometry3d` ist jetzt **opt-in im echten `CoupledMacWater3D`-Live-Step** angebunden. Mit `set_pcg_pressure_projection(true)` und `set_axis_vof_pcg_geometry(true)` werden vor jeder PCG-Druckprojektion aus dem aktuellen 3D-VOF-Feld die geometrisch auflösbaren achsparallelen Wasser/Luft-Grenzabstände rekonstruiert und in die MAC-Poisson-Matrix und die korrespondierenden Druckgradienten eingespeist. Das gilt auch für die optionale Projektion nach VOF-Transport. Jeder neue Projektionszeitpunkt löscht alte Grenzabstände. Ohne Opt-in bleibt der alte Solverpfad unverändert.

Die **Nasszellschwelle ändert sich im Opt-in explizit auf `f>0.5`**, passend zur geometrischen Grenzfläche; der Standardpfad verwendet `f>1e-5`. Nur streng achsparallele planare Schnitte werden akzeptiert. Bei schiefen, mehrdeutigen oder beidseitig partiellen Schnitten scheitert der verifizierte Step transaktional ohne Zustandsänderung. Der ungesicherte `step()` ist für diesen Modus verboten. Dies ist kein allgemeines PLIC und keine stresskonsistente Wasser/Luft-Zweiphasenphysik.

## Tatsächlicher Native-MSVC-Nachweis

- **5³ und 9³** hydrostatische 3D-Wassersäulen mit `f=0.75` im obersten nassen Voxel, `dx=1 m`, `rho=1000 kg/m³`. Unabhängige Referenz `p=rho*g*theta*dx`: `theta=0.25` ergibt **2452.5 Pa** im Live-PCG-Solver; der unveränderte Halbzell-Ghost-Druck des Legacy-PCG beträgt **4905 Pa**. Beide Werte werden im Native-Test gegen die analytische Vorgabe geprüft.
- Die 25 bzw. 81 geometrisch aufgelösten Grenzfaces sind in beiden Gittern nachgewiesen. Maximale nasse Divergenz: 1.97561e-17 bzw. 2.09703e-13 s^-1. Wassermasse: 43750 bzw. 141750 kg, innerhalb 1e-8 kg konserviert.
- Native Negativtests: Modus ohne PCG, ungesicherter Step, PCG-Abschaltung bei aktivem Modus, oblique VOF-Normale und doppelt partielle Grenze. Zustands- und Diagnose-Rollback, Reset der alten Grenzgeometrie im Folgeschritt.
- **43 native Assertions PASS**, vollständiger MSVC-Build erfolgreich, **122/122 CTest-Suites PASS**. Unabhängiger Python-Parser verwirft vier manipulierte Traces.
- Reproduzierbar: `tests/p2_live_axis_vof_pcg3d_tests.cpp`, `scripts/p2_live_axis_vof_pcg_partial_acceptance.py`. Gehashte Build-/Native-/CTest-/Gate-Logs und Quellen in `ENGINE TEST STUBE/runs/p2_live_axis_vof_pcg_partial.json`.

## Explizite Negativbefunde / offene Vollabnahme

Die **vorhandene** reale 6,00-s-MARIN/SPHERIC-Messvalidierung wird nicht künstlich neu gerechnet oder als PASS umgedeutet. Ihr historischer 32×10×10-Lauf liefert H1–H4 RMSE **0.136525 / 0.091500 / 0.148245 / 0.224399 m**, alle über der Grenze 0.05 m. Diese neuen synthetischen Hydrostatik-Tests ändern diese Messwerte nicht. Für P2 fehlen weiterhin unabhängige volle 6-s-Dreigitter-Konvergenz, geometrisch aufgelöste P1–P8-Wandtransducer, spannungskonsistente bewegte Wasser/Luft-Grenze, dynamische bidirektionale FSI und physikalische Gesamt-Massen-/Impuls-/Energiebilanz.

`scripts/phase_gate.py` meldet weiterhin **P2 INCOMPLETE / ALL_COMPLETE=False**. Der hier dokumentierte Nachweis ist ausschließlich ein abgegrenzter P2-Teilfortschritt.
