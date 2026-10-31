# P2 Teilnachweis: dreiachsiger symmetrischer PLIC-Transport (31.10.2026)

## Implementierung
- `include/upc/p2_plic_strang_periodic3d.hpp`: periodische Zentraldifferenzen für lokale VOF-Normalen, auch über alle Gitterränder; bei nicht rekonstruierbaren partiellen Zellen wird explizit abgebrochen.
- Symmetrische Strang-Folge X/2, Y/2, Z, Y/2, X/2. Jede aktive Stufe rekonstruiert die lokale Normale erneut und verwendet den bestehenden geometrischen PLIC-Slab-Flux aus `p2_plic_sheared_periodic3d.hpp`.
- Die drei vorgegebenen Face-Geschwindigkeitsfelder dürfen entlang der jeweiligen Transversalrichtungen variieren, müssen aber entlang ihrer eigenen periodischen Achse konstant sein. Das stellt für diese begrenzte Klasse diskrete Divergenzfreiheit sicher. Ein- und Ausfluss wird konservativ gebucht.
- Eingabe-Füllgrade und Face-Geschwindigkeiten werden nicht verändert. Ungültige Dimensionen, NaN, CFL-Verletzungen, nicht-solenoidale Lanes und unbestimmte Normalen werden zurückgewiesen.

## Echte native Messungen
- MSVC C++17 Release, 103/103 CTest erfolgreich. Neuer Test: `tests/p2_plic_strang_periodic3d_tests.cpp`, 8314 Assertions.
- Unabhängige analytische Referenz: exakt verschobene achsparallele 3D-Quader als Produkt der Intervallüberlappungen. Kein Referenzwert aus dem Solver abgelesen.
- 12×10×8: Anfangs-/Endvolumen jeweils 0,036 m³, L1-Fehler der Zellfüllgrade 0,44907557163397355, fünf aktive Stufen, maximaler Stufen-Courant 0,075.
- 20×16×12: Anfangs-/Endvolumen 0,18 m³, L1-Fehler 0,94311256547236577, fünf aktive Stufen, maximaler Stufen-Courant 0,075.
- Signwechselnde transversale Scherung in allen drei Geschwindigkeitskomponenten: 12×10×8 (0,192 m³) und 16×12×10 (0,576 m³), jeweils fünf Stufen und erhaltene Masse im Rundungsbereich.
- Periodischer Naht-Test mit zwei partiellen Grenzzellen, drei Raumachsen, Negativtests und Nullgeschwindigkeits-No-op.
- Reproduzierbarer Build: `scripts/p2_plic_strang_build.cmd`; maschinenlesbare Teilabnahme: `scripts/p2_plic_strang_acceptance.py`, Report `ENGINE TEST STUBE/runs/p2_plic_strang_partial.json`. MSVC-Build, CTest und Binärdatei sind separat gehasht.

## Grenzen und Vollphasenstatus
**P2 bleibt INCOMPLETE.** Das ist ein operatorgesplitteter, periodischer geometrischer Transport mit vorgegebenen Geschwindigkeiten, keine allgemeine unsplit dreidimensionale MAC/VOF-Navier-Stokes-Lösung. Die Normalen sind Gradientenapproximationen; Quader-Kanten zeigen messbare L1-Fehler. Nicht-periodische freie Oberflächen, Druck-/Geschwindigkeitsrückkopplung, dynamische Zweiwege-FSI, globaler Impuls-/Energieabschluss, externe CFD-Messdaten und erneute UE5.8-Native-E2E-Integration fehlen. P3–P8 bleiben ebenfalls vollphasenoffen.
