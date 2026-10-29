# P2 Teilabnahme: 3D donor-cell VOF auf stationaeren Cut-Cells (29.10.2026)

**Status: P2 PARTIAL, keine Vollphasenabnahme.**

## Implementierung
- Opt-in `P2StaticCutCellVof3D::step` transportiert Wasserfuellfraktionen in einem echten 3D-MAC-Gitter ueber alle drei Face-Richtungen (U/V/W). Der Solver benutzt die geometrischen Aperturen und offenen Zellvolumina des bestehenden `P2MovingBoxFaceApertures3D` mit **identischer Anfangs-/Endgeometrie**.
- Prescribed solenoidal MAC-Geschwindigkeiten [m/s] werden als integrierte Volumenfluesse [m^3] mit `dt * dx^2 * aperture` interpretiert. Die transportierte Wassermenge ist jeweils `volume_flux * upstream_fraction` (first-order upwind).
- Diskrete inkompressible Volumenbilanz pro Zelle, geschlossene Beckenraender, gesperrte Festkoerperflaechen, endliche Eingaben und lokale Donor-CFL <= 1 sind zwingend. Wasserinventar und Fuellfraktion werden pro Zelle begrenzt. Es gibt keine stille Normalisierung einer nichtdivergenzfreien Geschwindigkeit.
- Die Funktion aendert keine Eingaben und erhaelt das Gesamtwasservolumen innerhalb numerischer Rundungsfehler. Keine Solver-Integration, kein bewegter Festkoerper.

## Native Pruefungen
- `tests/p2_static_cut_cell_vof3d_tests.cpp`: 8^3 und 12^3, jeweils 40 Schritte mit gleichzeitig aktiven U/V/W-Fluessen aus zwei diskret divergentenfreien Wirbel-Streamfunktionen. Wasserinventar, Positivitaet und Fuellfraktionen [0,1] werden bei jedem Schritt kontrolliert.
- Unabhaengiger analytischer Upwind-Einzelschritt: `0.4 m/s * 0.1 s * 1 m^2 * 0.5 = 0.02 m^3` Wassermenge, also Donor 0.48 und Empfaenger 0.02 bei 1 m^3 Zellvolumen.
- Volle Einheitsfuellung als diskrete Invariante; echte teilweise blockierte Zellvolumina und Face-Aperturen eines stationaeren 3D-Quaders; acht Negativtests (Grenzfluss, Festkoerper, Divergenz, CFL, NaN, ungueltige Fuellung, Arraygroesse, dt=0).
- MSVC-Release, komplette CTest-Suite und reproduzierbarer JSON-Teilnachweis in `ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json` mit SHA256 von Quellcode und CTest-Log.

## Grenzen
- Nur passiver Skalartransport auf vorgeschriebenem, stationaerem MAC-Geschwindigkeitsfeld, **kein** eigenstaendiger Navier-Stokes-Druck-/Impulsschritt.
- Kein Moving-Cut-Cell-GCL, keine bewegte Festkoerper-Gegenreaktion, keine dynamisch berechnete Oberflaechenspannung, keine VOF-Interface-Rekonstruktion (nur diffusive first-order Upwind-Advektion).
- Keine globale physikalische Fluid-Festkoerper-Energie-/Impulserhaltung, keine unabhaengige 3D-CFD-Messvalidierung, kein UE5.8-Native-E2E fuer diesen Pfad.
- P2–P8 bleiben PARTIAL. Das vorhandene `phase_gate.py` darf diese Teilabnahme nicht als Full-PASS zaehlen.

## Zusatz: temporales CFL-Subcycling (29.10.2026)

- Die neue optionale Methode step_subcycled wiederholt den konservativen 3D-VOF-Operator mit 1 bis 1024 vom Aufrufer gewaehlten Teilzeitschritten. Der alte step bleibt unveraendert. Jeder Teilstep prueft die lokale Donor-CFL, MAC-Divergenz und Wasserbilanz.
- Native 8^3/12^3: 40-fache Wirbelgeschwindigkeit erzwingt Einzelschritt-CFL-Fehler. Vier Teilsteps werden zellweise mit vier separaten step-Aufrufen verglichen. Massenbilanz, Fraktionsgrenzen, 1-Step-Paritaet und ungueltige Substepzahlen werden getestet. Das JSON-Gate prueft die VOF_SUBCYCLE-Referenzzeilen.
- Kein adaptiver Fluid-Zeitschritt, keine dynamische MAC-Geschwindigkeitsberechnung, keine bewegte Festkoerper-Gegenreaktion. P2 bleibt PARTIAL.
