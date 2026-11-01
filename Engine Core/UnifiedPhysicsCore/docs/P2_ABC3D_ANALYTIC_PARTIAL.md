# P2 Teilnachweis: 3D-ABC-Navier-Stokes-Referenz (01.11.2026)

**Status: PARTIAL. Keine P2-Vollabnahme.**

## Physikalische Referenz

Das Arnold-Beltrami-Childress-Feld mit A=B=C=1, k=2π/L und L=1 m lautet:

- u(x,y,z,t) = a(t) [sin(kz)+cos(ky)]
- v(x,y,z,t) = a(t) [sin(kx)+cos(kz)]
- w(x,y,z,t) = a(t) [sin(ky)+cos(kx)]
- a(t) = a(0) exp(-ν k² t), a(0)=0.05 m/s, ν=0.003 m²/s.

Dieses Feld besitzt drei nichtverschwindende Geschwindigkeitskomponenten,
variiert entlang aller drei Achsen, ist divergenzfrei und erfüllt
curl(u)=k u. Wegen (u·∇)u=∇(|u|²/2) absorbiert der Druck
p=-ρ|u|²/2 (bis auf eine zeitabhängige Konstante) den nichtlinearen
Anteil. Die Laplace-Eigenfunktion liefert die exakte viskose
Amplitude. Dies ist eine **unabhängige kontinuierliche analytische**
3D-Navier-Stokes-Referenz, aber kein externer Messdatensatz.

## Echte Ausführung

- Reproduzieren: `cmd /d /c scripts\p2_abc3d_analytic_build.cmd`,
  anschließend `python scripts\p2_abc3d_analytic_acceptance.py`.
- MSVC C++17 Release, vorhandene vollständige 106 CTest-Suites.
- Drei periodische MAC-Gitter 8³, 12³, 16³, eine physikalische
  Zeiteinheit von 0.02 s; explizite Viskosität, nichtlineare
  Impulsadvektion, PCG-Druckprojektion und geometrischer PLIC-Transport
  eines Wasser-Slabs im selben Solver-Schritt.
- Pro einzigartiger MAC-Face-Komponente Vergleich zur exakten
  kontinuierlichen Geschwindigkeit zum Endzeitpunkt; L2 relativ,
  maximaler Absolutfehler, projizierte Divergenz, Wasser-Massenbilanz,
  kinetischer Energiequotient und Gitterverfeinerung.
- Der Referenz-Energiequotient ist exp(-2νk²t)=0.9952737937154058.
  Der tatsächliche numerische Quotient ist kleiner: zusätzliche
  Upwind-Dissipation, **keine** physikalische Energieerhaltungsaussage.

| Gitter | rel. L2 | max. | max. Divergenz (1/s) | Wasserbilanz (m³) | numerischer Energiequotient |
|---|---:|---:|---:|---:|---:|
| 8³ | 0.00177056535 | 0.00015005002 | 3.997e-15 | 1.388e-16 | 0.9918391192 |
| 12³ | 0.00126424513 | 0.00010574081 | 6.819e-15 | 5.551e-17 | 0.9928189620 |
| 16³ | 0.00097379950 | 0.00008259673 | 8.882e-16 | 2.776e-17 | 0.9933817816 |

Verfeinerungsfaktoren 1.40049 (8→12) und 1.29826 (12→16).
Die Fehler nehmen ab, aber nur ungefähr erster Ordnung, konsistent
mit der verwendeten Upwind-Impulsadvektion. Die L2-Werte sind
**nicht** als Messvalidierung oder allgemein konvergenter Zweiphasen-
Solver zu verstehen.

## Gefundene Blockade, nicht verschweigen

Ein separater Versuch über fünf 0.02-s-Schritte mit 3D-ABC und
periodischem Wasser-Slab erreichte auf 8³ zunächst einen Fehler
von 0.00872807; bei 12³ brach die PLIC-Normalenrekonstruktion
mit `Unresolved periodic PLIC interface` ab. Das wird **nicht**
als bestandener Mehrschritt-Benchmark gewertet. Der geometrische
Normalen-Rekonstruktionspfad muss für gemischte, flachgradientige
Zellen genauer untersucht werden. Es wird kein künstlicher
Normalen-Fallback und keine Massennachkorrektur als Erfolg verbucht.

## Nachweise und Grenzen

- Test: `tests/p2_abc3d_analytic_tests.cpp`
- Build: `scripts/p2_abc3d_analytic_build.cmd`
- Quantitative Teilabnahme: `scripts/p2_abc3d_analytic_acceptance.py`
- Hash-Report: `ENGINE TEST STUBE/runs/p2_abc3d_analytic_partial.json`
- Rohprotokolle: `p2_abc3d_{compile,native,ctest}.txt` im runs-Ordner.
- Alle Arbeiten innerhalb `D:\Projekt Engine`, kein Remote-Push.

P2 bleibt offen: spannungskonsistente Zweiphasengrenzen,
dynamische Zweiwege-Fluid/Festkörper-Kopplung, gemessene unabhängige
3D-CFD-Referenzen und physikalisch geschlossene Gesamtenergie-/
Impulsbilanz. P3–P8 bleiben ebenfalls offen; keine neue UE5.8-E2E-Abnahme.
