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

## Zusatz: passiver geloester Stoff auf 3D-VOF-Wasserfluesse (30.10.2026)

- Opt-in `step_passive_tracer` transportiert eine Konzentration in kg/m^3 **Fluessigwasser** mit exakt den bereits berechneten signierten U/V/W-Wasserfluesse des **einzelnen** stationaeren 3D-Cut-Cell-VOF-Schritts. Pro Face verwendet der Stofftransport die Upwind-Konzentration des Wasserdonors. Trockene Zellen duerfen keinen geloesten Stoff enthalten; kein Stofftransport durch feste oder geschlossene Randflaechen.
- Eigene Inventarbilanz in kg, Positivitaets- und Konzentrationsgrenzen, endliche Werte und falsche Eingabegroessen werden geprueft. Das vorhandene VOF-Ergebnis bleibt unveraendert. Eine bereits ueber mehrere Teilsteps aufsummierte Wasserfluss-Ledger darf **nicht** mit einer einzigen Ausgangskonzentration kombiniert werden; daher bewusst nur eine Einzelstep-API.
- Native MSVC Release: 87/87 CTest-Suites PASS. Insgesamt 234710 Assertions im betroffenen Native-Test. 8^3 und 12^3 mit je 40 Wasser-/Stoff-Transportsteps, unabhängige zellweise Bilanz aus U/V/W-Face-Ledger (max. Residuum 0 im ersten Schritt), Stoffmassen 55.0512 kg und 300.7536 kg. Analytischer Donor: 0.02 m^3 Wasser x 3 kg/m^3 = 0.06 kg geloester Stoff; vier Negativfaelle und ein fraktionaler Cut-Cell-Konstantkonzentrationstest.
- Nachweis: `scripts/p2_static_cut_cell_vof_acceptance.py`, `ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json`, vollständiges CTest-Protokoll mit Quell-/Log-SHA256.
- **P2-P8 weiterhin PARTIAL, ALL_COMPLETE=False.** Nur passiver konservativer Stofftransport mit vorgeschriebenen Geschwindigkeiten. Keine Diffusion, Reaktion, Waerme-/Boden-Kopplung, Navier-Stokes-Impulsintegration, FSI-Gegenreaktion, externe CFD-Messvalidierung oder neues Unreal-5.8-Native-E2E.


## Zusatz: adaptiver mehrstufiger 3D-Wasser-/Tracertransport (30.10.2026)

- P2StaticCutCellVof3D::step_passive_tracer_sequence_adaptive transportiert den geloesten passiven Stoff durch **jeden tatsaechlichen** geometrisch-CFL-begrenzten VOF-Teilstep mit zeitlich unterschiedlichen vorgeschriebenen MAC-U/V/W-Feldern. Die Donor-Konzentration wird nach jedem Teilschritt aktualisiert; ein aufsummiertes Wasser-Face-Ledger wird nicht faelschlich mit der Anfangskonzentration multipliziert.
- Native 8^3/12^3: vier ungleich lange Zeitscheiben, je sechs tatsaechliche Teilsteps, separat geplante manuelle Einzelschritt-Referenz inklusive Wasser-, Konzentrations- und kumulativer Face-Flux-Paritaet. 8^3 verwendet fraktionale 3D-Cut-Cells. Sechs negative Eingabefaelle und Einzelschritt-Paritaet. Gemessene Wasserinventare 129.3984 / 544.08 m^3, geloeste Stoffinventare 67.6088832 / 355.1616 kg; zellweises Paritaetsresiduum 0.
- MSVC Release und 87/87 native CTest-Suites PASS, 239230 Assertions im betroffenen Native-Test. JSON-/Quell-/CTest-Hash-Gate: scripts/p2_static_cut_cell_vof_acceptance.py und ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Weiterhin stationaere Geometrie, extern vorgeschriebene MAC-Geschwindigkeiten und passiver Stoff ohne Diffusion, Chemie oder Waerme. Keine dynamische Druck-/Impulsintegration, physikalische FSI-Gegenreaktion, externe 3D-CFD-Messreferenz oder UE5.8-Native-E2E fuer diesen Pfad.


## Signierte passive Stoff-Face-Fluesse (30.10.2026)

Die opt-in-Tracer-Ergebnisse enthalten jetzt u_tracer_flux_kg,
v_tracer_flux_kg, w_tracer_flux_kg. Jede Komponente ist der signierte
tatsaechliche Stoffmassentransfer ueber dieselbe U/V/W-Face wie der
VOF-Wasserfluss. Die adaptive Sequenz summiert die **jeweils neu berechneten**
Donor-Massentransfers, nicht eine Endkonzentration mal kumulierten Wasserfluss.

Die native Referenzpruefung rekonstruiert die Endmasse jeder 3D-Zelle aus
Anfangsmasse und Divergenz der kumulierten Face-Stoffmassen. Bei 8^3 und
12^3 sowie je sechs adaptiven Schritten liegt das maximale absolute
zellweise Residuum bei 2.0816681711721685e-17 kg; der maximale aeussere
Randfluss ist 0 kg. Zusaetzlich wurden die drei Face-Arrays gegen ein
separates Einzelschritt-Replay geprueft. Analytischer positiver U-Transport:
0.06 kg. Das ist eine diskrete Transportbilanz, keine Validierung einer
chemischen Reaktion, Navier-Stokes-Dynamik oder Fluid-Festkoerper-Impulsbilanz.
P2 und P3-P8 bleiben PARTIAL.

## 30.10.2026: opt-in konservative 3D-Tracerdiffusion auf stationaeren Cut-Cells

- P2CutCellTracerDiffusion3D::step koppelt den bestehenden konservativen
  3D-Upwind-Wasser-/Tracer-Einzelschritt an einen expliziten Fick-artigen
  Stoffaustausch. Die Wassermenge und die alte API bleiben unveraendert.
- Der Transport nutzt D*dt*dx*face_aperture*min(VOF_a,VOF_b) als
  angenaehertes benetztes Leitvolumen. Signierte U/V/W-Diffusionsmassen
  werden separat vom advektiven Ledger ausgegeben. Geschlossene
  Aussen- und Festkoerperwaende haben Nullfluss.
- Der lokale explizite Diffusions-CFL sum(conductance)/water_volume <= 1
  verhindert negative Konzentrationen; D=0 ist exakt kompatibel.
  Negative, NaN-/Inf-Diffusivitaeten, ungueltige Konzentrationen und
  instabile Diffusions-CFL werden abgelehnt.
- Unabhaengige diskrete Sechs-Nachbar-Referenz bei D=0.05 m2/s,
  dt=0.1 s, dx=1 m: Start 1 kg im Zentrum, 0.97 kg verbleiben,
  je 0.005 kg an sechs Nachbarn, CFL=0.03. Weitere Tests auf
  8^3/12^3, je 40 echte advektiv-diffusive Schritte, fraktionale
  Cut-Cells und zellweise Ledger-Bilanzen. Native CTest + SHA-JSON-Gate.
- Nur P2-Teilfortschritt: benetzte Apertur ist ein numerisches
  Interface-Proxy, kein aufgeloestes Zweiphasen-Diffusionsgesetz;
  vorgeschriebene MAC-Geschwindigkeiten und stationaere Geometrie.
  Keine dynamische Navier-Stokes-/FSI-Kopplung, keine physikalische
  Gesamtimpuls-/Energieerhaltung, externe CFD-Messvalidierung oder
  neue UE5.8-Native-E2E-Abnahme. P2-P8 bleiben PARTIAL.

## 30.10.2026: diagonal anisotrope 3D-Tracerdiffusion (Teilabnahme)

- Opt-in `P2CutCellTracerDiffusion3D::step_anisotropic` akzeptiert
  `{D_x,D_y,D_z}` in m2/s und verwendet den jeweiligen Koeffizienten
  ausschliesslich auf U-/V-/W-Faces. Der bestehende skalare `step`
  delegiert mit drei identischen Koeffizienten; bitgleiche Paritaet
  einschliesslich echter 3D-MAC-Advektion wird nativ geprueft.
- Unabhaengige diskrete 3D-Pulsreferenz auf 8^3 und 12^3:
  D=(0.01,0.02,0.03) m2/s, dt=0.1 s, dx=1 m, 1 kg
  Startstoff in einer voll benetzten Zelle. Erwartet: Zentrum 0.988 kg,
  sechs Nachbarn paarweise 0.001/0.002/0.003 kg, lokaler CFL=0.012.
  Signierte Face-Fluesse und zellweise U/V/W-Massenbilanz geprueft.
  Reine Y-Diffusion bei D_x=D_z=0, Null-D-Altpfad und vier ungueltige
  Diffusivitaets-/CFL-Faelle ebenfalls getestet.
- Echte Nachweise: `tests/p2_static_cut_cell_vof3d_tests.cpp`,
  `scripts/p2_static_cut_cell_vof_acceptance.py`, CTest-Protokoll und
  JSON mit SHA256. Das Modell bleibt explizit, diagonal und
  gitterachsenorientiert. Kein voller Diffusionstensor, keine
  experimentelle Bodenanisotropie oder Zwei-Phasen-Kalibrierung.
  **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.**


## 30.10.2026: konservatives Diffusions-CFL-Subcycling (Teilabnahme)

- Opt-in P2CutCellTracerDiffusion3D::step_diffusion_subcycled_anisotropic:
  genau ein vorgeschriebener 3D-MAC-Advektionsschritt, danach begrenzte
  explizite Diffusions-Teilsteps auf der resultierenden stationaeren
  Wasser-/Cut-Cell-Geometrie. Diagonal anisotrope Koeffizienten bleiben
  moeglich. Der konservative CFL-Oberwert beruecksichtigt das kleinste
  benetzte Cut-Cell-Volumen; maximal 1024 Teilsteps, Standardziel 0.8.
- Native MSVC Release: 87/87 CTest-Suites; 270388 Assertions im betroffenen
  Test. 8^3/12^3 mit analytischer zweistufiger 3D-Pulsreferenz:
  Zentrum 0.15625 kg, Gesamtmasse 1 kg, 2 Schritte, CFL 0.75,
  zellweises Ledger-Residuum 0 kg. Manuelles Replay und U/V/W-Fluxledger
  stimmen ueberein; Grenzwerte und unzureichendes Budget werden abgelehnt.
- Fraktionale Cut-Cells mit 0.784 m3 Anfangsvolumen: 3 Teilsteps,
  0.6377551020408162 maximale lokale CFL, erhaltene 0.784 kg Stoffmasse,
  maximale zellweise Bilanzabweichung 1.6653345369377348e-16 kg.
  Einfache 3D-MAC-Advektion bei niedriger CFL wird separat mit dem
  bisherigen Advektions-/Diffusionsoperator verglichen.
- Maschinenlesbare quantitative Teilabnahme mit Quell- und CTest-Hashes:
  scripts/p2_static_cut_cell_vof_acceptance.py,
  ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json.
- **P2-P8 PARTIAL, ALL_COMPLETE=False.** Das ist konservatives numerisches
  Stoff-Diffusionssubcycling nach einem Wasser-Advektionsschritt, kein
  gemeinsames physikalisches Fluid-/Boden-/Thermal-/FSI-Zeitintegrationsmodell,
  keine externen 3D-Messdaten und keine UE5.8-Native-E2E-Abnahme.

## 30.10.2026: heterogene zellweise Stoffdiffusion (Teilabnahme)

- Neuer opt-in P2CutCellTracerDiffusion3D::step_heterogeneous mit
  drei zellweisen diagonalen Diffusivitaetsfeldern D_x/D_y/D_z in m2/s.
  An Materialgrenzen wird der harmonische Face-Mittelwert verwendet,
  bei D=0 ist die Face-Diffusion gesperrt. Benetzte stationaere
  Cut-Cell-Aperturen und konservative signierte U/V/W-Stofffluesse
  bleiben erhalten; lokale explizite CFL muss <=1 sein.
- Native analytische Referenz auf 8^3 und 12^3, dt=0.1 s:
  D_x=(1,2,4) fuer drei aufeinanderfolgende Zellen,
  D_y=D_z=0. Harmonik D_left=4/3, D_right=8/3.
  Zentrum=0.6000000000000001 kg, links=0.13333333333333333 kg,
  rechts=0.26666666666666666 kg; 1 kg Gesamtmasse und CFL=0.4.
  Signierte U-Fluesse -2/15 und +4/15 kg; unabhaengige
  zellweise U/V/W-Finite-Volumen-Bilanz max. 1.11e-16 kg.
  Uniforme Koeffizienten stimmen mit dem vorhandenen anisotropen
  Operator ueberein. Vier Negativtests: Feldgroesse, negatives D,
  NaN-D, instabile CFL.
- MSVC Release und vollstaendiger CTest, native Assertions und
  JSON/SHA256-Pruefung: scripts/p2_static_cut_cell_vof_acceptance.py;
  ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json.
- **P2-P8 bleiben PARTIAL, ALL_COMPLETE=False.** Keine kalibrierten
  Bodenparameter, keine bewegte Geometrie, keine physikalische
  Fluid-/Festkoerper-Impuls- und Energiebilanz, keine unabhaengige
  gemessene 3D-CFD-Validierung oder neue UE5.8-Native-E2E.

## 30.10.2026: heterogenes Diffusionssubcycling mit lokaler CFL (Teilabnahme)

- Neuer opt-in P2CutCellTracerDiffusion3D::step_diffusion_subcycled_heterogeneous kombiniert einen vorgeschriebenen MAC-VOF-/Tracer-Advektionsschritt mit mehreren expliziten konservativen Diffusionsschritten fuer zellweise unterschiedliche D_x/D_y/D_z. Die Substepzahl folgt der tatsaechlichen Summe der harmonischen benetzten Face-Leitwerte pro Wasservolumen, nicht einem globalen D-Maximum; maximales Budget 1024.
- Nach jedem Diffusionssubstep wird die Konzentration aktualisiert. Die U/V/W-Face-Stoffmassenledger werden kumuliert. Unveraenderte Festkoerpergeometrie, null Aussenfluss und Null-D-Paritaet. Eingabevalidierung: Feldlaengen, D<0/NaN/Inf, CFL-Ziel und Teilstepbudget.
- Native 8^3 und 12^3: ein Einzelschritt dt=0.5 s mit lokaler CFL 2.0 wird abgelehnt; die neue Methode verwendet je 3 Substeps (CFL 2/3), bewahrt 1 kg Stoff und stimmt mit drei manuell ausgefuehrten heterogenen Referenzschritten einschliesslich kumulierter U/V/W-Face-Ledger ueberein. Maximales unabhaengiges zellweises Massenresiduum 5.5511151231257827e-17 kg.
- Echte fraktionale 3D-Cut-Cells (8 pro Gitter) auf 8^3/12^3: je 2 Substeps, lokale CFL 0.54728663510270337 / 0.63530549094953981, Stoffmasse 113.00864000000004 / 504.52863999999994 kg, zellweises Residuum 1.1102230246251565e-16 kg. Die Stoffmassen folgen dem tatsaechlichen freien Zellvolumen.
- MSVC Release, 87/87 CTest, 288988 native Assertions. Quantitatives Gate scripts/p2_static_cut_cell_vof_acceptance.py und JSON ENGINE TEST STUBE/runs/p2_static_cut_cell_vof_partial.json mit Quell-SHA256 und CTest-Log-SHA256.
- **Keine P2-Vollabnahme**: keine dynamische Navier-Stokes-/Fluid-Festkoerper-Gegenreaktion, keine Gesamtimpuls-/Energiebilanz, keine externe 3D-CFD-Messvalidierung und kein neuer UE5.8-Native-E2E-Lauf. P2-P8 bleiben PARTIAL.

## 30.10.2026: numerisch robuster harmonischer Materialgrenzwert (Teilabnahme)

- Heterogene 3D-Cut-Cell-Stoffdiffusion nutzt jetzt ein skaliertes
  harmonisches Mittel min(Da,Db)/(0.5+0.5*min(Da,Db)/max(Da,Db)).
  Dadurch erzeugen sehr kleine positive D keine falsche Nullleitfaehigkeit
  durch 1/D-Ueberlauf; grosse endliche D vermeiden 2*D-Ueberlauf.
  Beide heterogenen Operatoren (Einzelschritt und CFL-Subcycling) verwenden
  dieselbe numerisch stabile Face-Regel. D=0 bleibt undurchlaessig.
- Unabhaengige Grenzwertreferenzen auf 8^3/12^3 mit ruhendem MAC-Feld:
  D_links=1e-310, D_rechts=1e308, dt=1e300 s: signierter U-Face-Flux
  2e-10 kg; gleiche subnormale D: 1e-10 kg; gleiche grosse D=1e308,
  dt=1e-310 s: 0.01 kg. Positive Massenfluesse, CFL, Erhaltung und
  Subcycling-Paritaet werden im nativen Test und quantitativen Gate geprueft.
  Extreme dt/D sind absichtliche numerische Stresstests, keine
  realistischen Material- oder Zeitskalen.
- **P2-P8 bleiben PARTIAL; ALL_COMPLETE=False.** Keine echte
  Fluid-Festkoerper-Kopplung, physikalische Gesamtenergie-/Impulsbilanz,
  externen 3D-CFD-Messdaten oder neue UE5.8-Native-E2E-Abnahme.

## 30.10.2026: diskrete heterogene Tracer-Mischungsbilanz (Teilabnahme)

- Der heterogene CFL-Subcycling-Operator liefert einen numerischen
  Mischungs-Audit mit diffusive_quadratic_audited sowie
  diffusive_quadratic_before_kg2_m3,
  diffusive_quadratic_after_kg2_m3 und
  diffusive_quadratic_loss_kg2_m3. Definition:
  Q = Summe ueber Zellen [alpha * dx^3 * VOF * c^2].
  Einheit kg^2/m^3. Die Messung beginnt **nach** der vorgeschriebenen
  MAC-Advektion und endet nach den Diffusions-Teilzeitschritten.
- Mathematischer Grund: Symmetrische harmonische Face-Leitwerte und
  geschlossene Randflaechen ergeben bei lokaler CFL <=1 einen
  volumen-gewichteten Mittelungsoperator. Konvexitaet von c^2
  erzwingt Q_nach <= Q_vor; Verletzung oberhalb der Rundungstoleranz
  wird zur Laufzeit abgelehnt. Keine Aenderung an den MAC-Impulsen,
  Wasser-VOF, Temperatur oder Festkoerpern.
- MSVC-Release-Nativetest auf 8^3 und 12^3: unabhaengige exakte
  Dreizell-Referenz (harmonische Transferanteile 2/9 und 4/9 pro
  Substep). Nach 3 Schritten Konzentrationen (66,85,92)/243;
  Q_vor=1, Q_nach=20045/59049, Q-Verlust=39004/59049.
  Echte fraktionale Cut-Cells auf beiden Gittern:
  26.555084800000003 -> 26.501312115821687 und
  158.4486847999996 -> 158.21113753694465.
  Zusaetzliche unabhaengige Wet-Volume-Q-Referenz und Null-D-
  Legacy-Paritaet. Nachweis ueber dasselbe CTest-/JSON-/SHA256-Gate.
- **Nur P2-Teilfortschritt.** Q ist weder physikalische kinetische
  Energie noch thermodynamische Entropie. Kein Nachweis bewegter
  Fluid-Festkoerper-Interaktion, vollstaendiger Navier-Stokes-Physik,
  externer 3D-CFD-Messdaten oder neuer UE5.8-Native-E2E-Abnahme.

## 30.10.2026: exakte diskrete Diffusions-Q-Identitaet (Teilabnahme)

- Fuer jeden heterogenen 3D-Diffusions-Teilzeitschritt werden die
  **vorherigen** Zellkonzentrationen, signierten U/V/W-Face-Fluesse
  und die benetzten Cut-Cell-Volumina getrennt bilanziert.
  Exakte explizite Euler-Identitaet:
  Q_vor - Q_nach = 2 * Summe_Faces[J * (c_links-c_rechts)]
                   - Summe_Zellen[V_wet * (c_neu-c_alt)^2].
  Die ersten beiden Terme werden als
  diffusive_quadratic_face_drive_kg2_m3 und
  diffusive_quadratic_euler_correction_kg2_m3 ausgegeben.
  diffusive_quadratic_identity_residual_kg2_m3 kontrolliert die
  Schliessung; Abweichungen oberhalb der numerischen Toleranz
  verursachen einen Fehler statt einer falschen Erfolgsmeldung.
- Analytisch unabhaengige 3-Zell-Euler-Referenz auf 8^3 und 12^3,
  drei Schritte, 2/9- und 4/9-Face-Transfers:
  Face-Drive=1.356500533455266,
  Euler-Korrektur=0.6959643685752509,
  Q-Verlust=39004/59049.
  Fraktionale 3D-Cut-Cells pruefen ebenfalls beide Terme und
  die Schliessung. Reale native Tests und quantitativer
  Source-/CTest-SHA256-Gate erforderlich.
- **Keine physikalische Energiebilanz**: Der Ausdruck ist ein
  numerisches Mischungsfunktional passiver gelöster Stoffe.
  Keine bewegten Koerper, externe 3D-CFD-Messvalidierung oder
  neue UE5.8-Native-E2E-Abnahme. P2-P8 bleiben unvollstaendig.