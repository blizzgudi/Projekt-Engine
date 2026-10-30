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

## Zusatz: nichtuniforme MAC-Zeitfolgen mit gemeinsamem CFL-Budget (30.10.2026)

- step_velocity_sequence_adaptive verarbeitet vorgeschriebene U/V/W-MAC-Felder mit unterschiedlichen positiven Zeitscheibendauern. Pro Scheibe wird eine geometrisch sichere Donor-CFL-Teilstepzahl gewaehlt; ein gemeinsames Budget (maximal 1024) verhindert unbegrenzte Arbeit.
- Native 8^3/12^3: vier ungleiche Zeitscheiben mit wechselnden dreiachsigen Wirbelfeldern, unabhaengige manuelle CFL-/Teilstep-Referenz, exakte Zellparitaet, Massenbilanz und Fuellgrenzen. Spaetere NaN-/Divergenz-/Zeitfehler und unzureichendes Gesamtbudget werden abgewiesen.
- Die bisherigen Methoden bleiben unveraendert. Dies ist keine Navier-Stokes-Zeitintegration und keine bewegte FSI. P2-P8 bleiben PARTIAL.

## Zusatz: signiertes 3D-Face-Wasservolumen-Ledger (30.10.2026)

- Der opt-in P2StaticCutCellVof3D-Operator gibt nun fuer jede U/V/W-Face das tatsaechlich mit first-order Upwind transportierte, vorzeichenbehaftete Wasservolumen [m^3] zurueck. Stationaere/versiegelte Randflaechen bleiben null.
- Subcycling, konstante und zeitlich variable Sequenzen summieren die Face-Volumina ueber alle real ausgefuehrten Teilschritte. Dadurch ist eine unabhängige zellweise Finite-Volumen-Nachrechnung des gesamten Wasserinventars moeglich, ohne auf den internen Zell-Akkumulator zuzugreifen.
- Native Pruefungen auf 8^3/12^3, vier Zeitintegrationsmodi sowie ein fraktionales 3D-Cut-Cell-Gitter: positive/negative U/V/W-Fluesse, Formate, geschlossene Aussenraender, zellweise Wasserbilanz und globale Konservierung. Das maschinenlesbare Teilgate prueft die gemessenen Residuen und Source-/CTest-SHA256.
- Dies ist **keine** dynamische MAC-Impulsadvektion, keine physikalische FSI-Gegenreaktion, keine Energieerhaltung der bewegten Festkoerper und kein P2-Vollphasenabschluss. P2-P8 bleiben PARTIAL.

## Grenzen
- Nur passiver Skalartransport auf vorgeschriebenem, stationaerem MAC-Geschwindigkeitsfeld, **kein** eigenstaendiger Navier-Stokes-Druck-/Impulsschritt.
- Kein Moving-Cut-Cell-GCL, keine bewegte Festkoerper-Gegenreaktion, keine dynamisch berechnete Oberflaechenspannung, keine VOF-Interface-Rekonstruktion (nur diffusive first-order Upwind-Advektion).
- Keine globale physikalische Fluid-Festkoerper-Energie-/Impulserhaltung, keine unabhaengige 3D-CFD-Messvalidierung, kein UE5.8-Native-E2E fuer diesen Pfad.
- P2–P8 bleiben PARTIAL. Das vorhandene `phase_gate.py` darf diese Teilabnahme nicht als Full-PASS zaehlen.

## Zusatz: temporales CFL-Subcycling (29.10.2026)

- Die neue optionale Methode step_subcycled wiederholt den konservativen 3D-VOF-Operator mit 1 bis 1024 vom Aufrufer gewaehlten Teilzeitschritten. Der alte step bleibt unveraendert. Jeder Teilstep prueft die lokale Donor-CFL, MAC-Divergenz und Wasserbilanz.
- Native 8^3/12^3: 40-fache Wirbelgeschwindigkeit erzwingt Einzelschritt-CFL-Fehler. Vier Teilsteps werden zellweise mit vier separaten step-Aufrufen verglichen. Massenbilanz, Fraktionsgrenzen, 1-Step-Paritaet und ungueltige Substepzahlen werden getestet. Das JSON-Gate prueft die VOF_SUBCYCLE-Referenzzeilen.
- Kein adaptiver Fluid-Zeitschritt, keine dynamische MAC-Geschwindigkeitsberechnung, keine bewegte Festkoerper-Gegenreaktion. P2 bleibt PARTIAL.

## Zusatz: automatische geometrische CFL-Teilzeitschritte (29.10.2026)

- Optionale Methode P2StaticCutCellVof3D::step_auto_subcycled waehlt aus dem maximalen geometrischen Donor-CFL die kleinste ganze Teilstepzahl fuer das vorgegebene Ziel (Default 0.8, Budget maximal 1024).
- Ein kurzer Pilot-Step validiert die bestehenden stationaeren Cut-Cell-, geschlossenen Rand- und Solenoidalbedingungen. Bei unzureichendem Budget oder ungueltigen Parametern wird explizit abgebrochen, nicht stillschweigend ueber die CFL-Grenze gegangen.
- Native 8^3/12^3 mit 40-fachen U/V/W-Geschwindigkeiten: automatisch zwei Teilsteps bei CFL 0.8; zellweise bitgleicher Vergleich mit explizit zwei Substeps, Wasservolumen invariant, kleiner Zeitschritt bleibt bei einem Step, acht negative Kontrollfaelle.
- MSVC Release und komplette 87-Suite-CTest sowie separates JSON-Gate mit zwei automatischen Referenzzeilen und SHA256 der tatsaechlich getesteten Quellen. Die quantitative Abnahme gilt nur fuer vorgeschriebene MAC-Geschwindigkeiten und stationaere Festkoerper.
- **P2 weiterhin PARTIAL:** Die Methode ist ein geometrischer CFL-Controller, kein adaptiver Navier-Stokes-/VOF-/FSI-Solver; dynamischer Druck, bewegte Festkoerper-Gegenreaktion, physikalische Energie-/Impulsbilanz, externe CFD-Messdaten und UE5.8-Native-E2E fehlen.
## Zusatz: zeitlich variable vorgeschriebene 3D-MAC-Felder (29.10.2026)

- Opt-in step_velocity_sequence akzeptiert je gleichlangem Teilstep ein anderes U/V/W-Feld. Jede Geschwindigkeit durchlaeuft die vorhandenen Cut-Cell-, Divergenz-, CFL- und Volumenchecks.
- Native 8^3/12^3: vier wechselnde dreiachsige Wirbelfelder, exakter Vergleich gegen unabhaengige manuelle Einzelsteps, Wassererhaltung, Fraktionsgrenzen, ungueltige spaetere Steps und leere/ungleiche Sequenzen.
- Das quantitative JSON-Gate prueft zwei VOF_SEQUENCE-Zeilen samt Quell-SHA256. Keine dynamische Navier-Stokes-Berechnung, keine bewegte FSI oder UE5.8-Native-E2E. P2-P8 bleiben PARTIAL.
