# P2-Teilnachweis: Periodische Druckprojektion vor geometrischem MAC/PLIC-Transport

Stand: 2026-10-31. **P2 bleibt INCOMPLETE. P3–P8 bleiben INCOMPLETE.**

## Abgegrenzter neuer Adapter

`include/upc/p2_projected_plic_schedule3d.hpp` verbindet zwei vorhandene, zuvor separat getestete Bausteine: den konstantdichten periodischen 3D-MAC-Druckprojektor und den zeitlich gestaffelten geometrischen VOF/PLIC-Transport. Ein Segment akzeptiert echte staggered MAC-Face-Arrays mit den Dimensionen (nx+1)×ny×nz, nx×(ny+1)×nz und nx×ny×(nz+1). Nach dem Projektionsschritt werden die korrigierten Geschwindigkeiten explizit auf die positiven Zellflächen des PLIC-Operators abgebildet. Für jedes Zeitsegment werden die Anfangs- und Enddivergenz sowie PCG-Iterationen protokolliert. Die Eingaben bleiben unverändert; Fehler geben kein Teilergebnis zurück.

Die Druckprojektion entfernt den diskreten Gradientenanteil einer **vorgegebenen** Geschwindigkeit. Sie berechnet keine Navier-Stokes-Vorhersage aus Kräften, keine wasserstandsabhängige Zweiphasen-Druckgleichung und keine dynamische FSI. Der periodische Projektor ist auf höchstens 32.768 Zellen begrenzt. Die PLIC-Advektion nutzt weiterhin geometrisch approximierte, gleichzeitig bilanzierte Face-Slabs und kann bei unbestimmbaren Normalen oder Überlappung abbrechen. Numerische Projektions-Rundungsreste unter 1e-13 m/s werden vor der Advektion zu null gesetzt; der Transport prüft anschließend die Divergenz erneut. Das ist keine physikalische Geschwindigkeitsuntergrenze.

## Native Prüfung

`tests/p2_projected_plic_schedule3d_tests.cpp` prüft einen analytisch hergestellten 3D-Geschwindigkeitsgradienten auf einem 8³-Periodengitter. Die Soll-Drucklösung ist der bekannte trigonometrische Skalar `p=rho/dt*phi` (Nullmittel-Gauge); die Druckprojektion muss die vorgegebene konstante X-Geschwindigkeit wiederherstellen und beide anderen Komponenten auf null bringen. Zwei verschiedene Zeitintervalle werden gegen einen **separat** ausgeführten PLIC-Transport mit den analytisch bekannten Grundgeschwindigkeiten verglichen. Eine zweite Probe prüft ein entlang Y geschertes, exakt divergenzfreies 3D-MAC-Feld und die korrekte staggered-zu-Plusface-Abbildung. Zusätzlich: Wasserbilanz, Zellfüllgrenzen, Segment-Substeps, Input-Unveränderlichkeit sowie Negativtests für falsche Arrays, NaN, Periodennaht, ungültige Dichte/Zeit und erschöpftes CFL-Budget.

Reproduktion unter `D:\Projekt Engine`: `Engine Core\UnifiedPhysicsCore\scripts\p2_projected_plic_build.cmd` kompiliert den neuen Test separat mit MSVC C++17 und führt danach den bestehenden CTest-Regressionssatz aus. Der neue Native-Test ist **nicht** im bestehenden CMake/CTest-Register; die getrennte Ausführung und ihre Logs werden ausdrücklich nachgewiesen, um ältere Quellenfingerprints nicht unnötig zu invalidieren. `scripts/p2_projected_plic_acceptance.py` validiert die Ergebnisse und schreibt einen SHA256-gebundenen Teilreport nach `ENGINE TEST STUBE/runs/p2_projected_plic_partial.json`.

## Vollphasenlücken

Keine echte freie Oberfläche mit spannungskonsistenter Druck-/Viskositätskopplung, kein dynamischer Zweiwege-Fluid-Festkörper-Impulsaustausch, keine externen 3D-CFD-Messdaten und keine globale physikalische Energie-/Impulsabnahme. Wasser/Boden, Wärme, Festkörper, Reaktionen, GPU/LOD und Unreal-5.8-Native-E2E werden durch diesen isolierten Adapter nicht zusätzlich abgenommen. Ein grüner Teilreport darf den Phasenstatus nicht verändern.
