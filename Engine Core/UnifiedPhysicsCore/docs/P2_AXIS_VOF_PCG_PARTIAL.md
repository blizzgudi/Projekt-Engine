# P2 Teilnachweis: axis-aligned VOF zu geometrischem MAC-PCG-Druckabstand

Stand: 01.11.2026. **P2 weiterhin PARTIAL.** Die bestehende freie Oberfläche ist weder stresskonsistent noch eine validierte Zweiphasenströmung.

## Implementierter abgegrenzter Schritt

Der neue Header `include/upc/p2_axis_vof_pcg_geometry3d.hpp` verbindet tatsächliche 3D-VOF-Füllgrade und die vorhandene lokale Normalenrekonstruktion mit dem zuvor separat getesteten geometrischen MAC-PCG-Druckabstand. Für **streng achsparallele ebene** Grenzflächen berechnet er aus einem einzigen teilweise gefüllten Zellvolumen den Abstand vom nassen Zellzentrum zur Druckgrenze. Für eine nasse Teilzelle mit f=0,75 folgt theta=0,25; für eine benachbarte trockene Teilzelle mit f=0,25 folgt theta=0,75. Die zugehörigen Druckwerte werden in x/y/z auf echten MAC-Faces projiziert.

Die Normalen werden aus VOF-Nachbarzellen rekonstruiert und auf die vorgegebene Achsrichtung geprüft. Schräg orientierte Gradienten und zwei gleichzeitig teilgefüllte Zellen werden abgewiesen. Reine 1/0-Zellgrenzen sind geometrisch **nicht aufgelöst** und verbleiben beim dokumentierten Halbzell-Default. Die Auflösung benutzt **explizit einen 0,5-Nass-Schwellenwert**; die bisherige Live-Wasserimplementierung verwendet eine andere Nassmaske. Der neue Pfad wird deshalb bewusst **nicht** unvalidiert in die MARIN-Live-Simulation aktiviert.

`MacProjection3D::clear_interface_distances()` entfernt alte Grenzflächenabstände, bevor eine neue VOF-Geometrie angewendet wird. Eine zweite Projektion mit unaufgelöster Geometrie darf keine Werte des vorigen Frames erben. Ungültige Werte werden vor der Rücksetzung abgewiesen.

## Native QA, Grenzen und nächste Schritte

Der neue MSVC-Test `tests/p2_axis_vof_pcg_geometry3d_tests.cpp` deckt x/y/z, zwei echte 3D-Gitter (5³/9³), beide Orientierungen und zwei Fülllagen ab: 24 unabhängig prüfbare Druckfälle. Die hergeleitete Impulsreferenz lautet p = -rho * dx * theta / dt, hier -2.500 bzw. -7.500 Pa. Zusätzlich geprüft: projizierte Divergenz, Nullnormalfluss, zurückgesetzte Geometrie (-2.500 auf -5.000 Pa), ungültige Schwellenwerte, doppelte Teilzellen, schräge Normalen und NaN. Ein eigenes Python-Audit prüft echte native Logs, den vollständigen CTest-Lauf, MSVC-Build, SHA256 und den negativen Phase-Gate-Status; vier absichtlich korrumpierte Native-Logs müssen abgewiesen werden.

Die **alten negativen experimentellen MARIN/SPHERIC-P1–P8-Druckabweichungen bleiben unverändert**. Kein neuer 6-s-Messlauf, keine unabhängige dritte Gitterauflösung und keine echte Luftspannung, Oberflächenspannung oder dynamische bidirektionale FSI wurden in diesem Teilnachweis erbracht. Der nächste fachliche Schritt ist eine allgemeine geometrische PLIC-Grenzfläche samt konsistenter Live-Nassmaske, gefolgt von realer H1–H4/P1–P8-Messkonvergenz.

Belege: `ENGINE TEST STUBE/runs/p2_axis_vof_pcg_native.txt`, `p2_axis_vof_pcg_ctest.txt`, `p2_axis_vof_pcg_full_build.txt`, `p2_axis_vof_pcg_phase_gate.txt`, `p2_axis_vof_pcg_mutations.txt`, `p2_axis_vof_pcg_partial.json`; reproduzierbarer Prüfer: `scripts/p2_axis_vof_pcg_partial_acceptance.py`.
