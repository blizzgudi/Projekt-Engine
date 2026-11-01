# P2 Teilabnahme: wassergewichtetes 3D-MAC-Impuls-/Energieinventar

Stand: 2026-11-01. **P2 bleibt INCOMPLETE.** Der neue optionale Header
include/upc/p2_water_mac_inventory3d.hpp berechnet aus einem
dreidimensionalen periodischen MAC-Geschwindigkeitsfeld und dem
geometrisch transportierten VOF-Wasserfüllgrad eine *wassergewichtete*
Massen-, Impuls- und kinetische Energiequadratur in SI-Einheiten.

Die Zellgeschwindigkeit ist das arithmetische Mittel der zwei zugehörigen
MAC-Faces. Die Zellmasse ist rho * dx^3 * fill. Pro Zelle werden
m * u und m * |u|^2 / 2 summiert. Periodische Duplikat-Faces werden
nicht doppelt gezählt. Ungültige Füllgrade, Nahtgeschwindigkeiten,
Dimensionen, Einheiten und nichtfinite Werte werden zurückgewiesen.

Der separate Endpunktadapter vergleicht Anfangs- und Endzustand einer
tatsächlich fortgeschriebenen viskosen MAC/PLIC-Sequenz. Er vergleicht
die Wassermassen zusätzlich mit dem vorhandenen Sequenzinventar. Die
Änderungen von Impuls und kinetischer Energie sind *Messgrößen*, nicht
automatisch Erhaltungsfehler: Das Wasser bewegt sich im derzeitigen
Modell als passiver Füllgrad in einem konstantdichten Vollraum-Fluid.

## Abnahme und reproduzierbare Belege

- scripts/p2_water_mac_inventory_build.cmd kompiliert einen eigenständigen
  nativen MSVC-C++17-Release-Test und führt die bestehende komplette
  CTest-Regression aus.
- scripts/p2_water_mac_inventory_acceptance.py vergleicht die nativen
  Messungen gegen unabhängige Python-Formeln für 8³ und 12³:
  konstante Geschwindigkeiten bei 25 % Füllung, einseitig gefüllte
  periodische Sinus-Scherung, dreikomponentiges divergenzfreies
  Scherfeld mit fraktionalem Füllgrad und ein echter dreischrittiger
  viskoser MAC/PLIC-Wassertransport. Zusätzlich Negativtests.
- Bericht: ENGINE TEST STUBE/runs/p2_water_mac_inventory_partial.json
  mit Quell-, Binär- und Protokoll-SHA256, nur bei echten PASS-Ergebnissen.

**Grenzen:** Zellmittelung ist keine geometrisch exakte subzelluläre
Wasser-Impulsintegration. Keine wassergewichtete dynamische
Impulsadvektion, keine variable Dichte/Zweiphasen-Spannungsbilanz,
keine FSI-Gegenkraft, keine thermische Gesamtenergie und keine
unabhängigen gemessenen CFD-Referenzen. Kein Unreal-5.8-Native-E2E
für diesen neuen Diagnosepfad. **P2-P8 bleiben INCOMPLETE.**
