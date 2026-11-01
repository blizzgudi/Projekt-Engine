# P2 Teilnachweis: periodische 3D-MAC-Bulk-Impuls-/Energieinventur (01.11.2026)

**Status: PARTIAL, P2 NICHT abgeschlossen.** Der optionale Sequenzadapter
`p2_viscous_mac_plic_sequence3d` gibt nun fuer jedes Zeitintervall
SI-Diagnosen fuer die drei Impulskomponenten (kg m/s) und die kinetische
Energie (J) sowie die Anfangs-/Endwerte und die Teleskop-Residuen aus.

Die diskrete Quadratur summiert ausschliesslich die **eindeutigen**
periodischen MAC-Faces je Achse (nx*ny*nz), ohne die duplizierten
Naht-Faces doppelt zu zaehlen. Ein Face traegt `rho*dx^3*v` zum
Komponentenimpuls und `0.5*rho*dx^3*v^2` zur kinetischen Energie bei.
Ungueltige Dimensionen, Einheiten, nichtfinite Werte und
nichtperiodische Naehte werden explizit abgelehnt. Der Adapter fuehrt
die Intervalldifferenzen zu einem Teleskop-Check zusammen.

## Reproduzierbare Tests

`scripts/p2_mac_bulk_budget_build.cmd` kompiliert den separaten
MSVC-C++17-Native-Test, fuehrt ihn aus und wiederholt den vorhandenen
CTest-Regressionssatz. `scripts/p2_mac_bulk_budget_acceptance.py`
prueft die Protokolle gegen eine **unabhaengig berechnete**
diskrete Fourier-Scherreferenz auf 8^3 und 12^3 Gittern, den
periodischen Mittelimpuls, Energieverlust bei Viskositaet,
Nullviskositaets-Invarianz, einen gemischten 3D-Wasserblock, acht
Negativfaelle und Quell-/Binaer-SHA256. Die maschinenlesbare Teilabnahme
liegt in `ENGINE TEST STUBE/runs/p2_mac_bulk_budget_partial.json`.

Die Testreferenz verwendet
`v_x(j,t)=0.02+0.04*factor(t)*sin(2*pi*j/n)`, mit
`factor(t+dt)=factor(t)*(1-4*alpha*sin(pi/n)^2)^substeps`.
Damit sind `P_x=rho*dx^3*n^3*0.02` und
`K=0.5*rho*dx^3*n^3*(0.02^2+0.04^2*factor^2/2)`.
Diese Formeln stehen unabhaengig vom Produktivcode im Test und
im Python-Gate.

## Geltungsgrenzen

Diese Bilanz ist **nur fuer die konstante Dichte des gesamten
periodischen Modellfluids** physikalisch in SI skaliert.
Das Modell transportiert VOF passiv; die kinetische Energie wird
nicht mit Wasseranteilen gewichtet. Daher ist dies **keine**
Wasser-/Luft-Zweiphasen-Gesamtenergie, keine physikalisch geschlossene
Energiebilanz mit Waerme oder Schwerkraft, kein Beweis globaler
Impulserhaltung des nichtlinearen Diskretisierungsoperators und keine
Fluid-Festkoerper-Gegenreaktion. Die Teleskop-Residuen testen
Buchungskonsistenz, nicht automatisch Erhaltung. P2–P8 bleiben offen;
keine neue UE5.8 Native-E2E-Abnahme.
