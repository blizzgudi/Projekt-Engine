# P2 Teilabnahme: VOF-gewichtete Zellmittelpunkt-Energie (31.10.2026)

**Status: PARTIAL. P2-P8 INCOMPLETE; ALL_COMPLETE=False.**

## Gegenstand

Neue read-only Funktion `vof_mechanical_inventory3d` und Live-Adapter
`CoupledMacWater3D::water_mechanical_inventory()`. Der Operator gewichtet
die aus den jeweils zwei angrenzenden MAC-Faces gemittelten
Zellmittelpunkt-Geschwindigkeiten mit der tatsaechlichen VOF-Zellmasse
`m_i=rho*dx^3*f_i`. Er berechnet `sum(m_i)`,
`sum(m_i*v_i)`, `sum(0.5*m_i*|v_i|^2)` und
`sum(m_i*9.81*(z_i+0.5)*dx)`. Trockene Zellen/Faces zaehlen nicht.
Das Ergebnis ist ein **zellzentrierter Naeherungswert**, keine exakte
staggered-MAC-Energiequadratur und kein Erhaltungsbeweis.

Der Operator ist ohne zusaetzliche Speicherung oder Solver-Modifikation
abrufbar. Er prueft die Zellbelegung auf [0,1], Festkoerper-Ueberlappung,
gueltige Parameter und finite Ergebnisse. Kein neues Runtime-Flag.

## Echte native Pruefung

MSVC Release-Build und **94/94 CTest PASS**.
Native `upc_p2_vof_mechanical_inventory_tests.exe`:
**62 Assertions**, zwei unabhaengige 3D-Raster 8^3 und 12^3.

Analytische Zwei-Zellen-Referenz mit dx=0.1 m, rho=1000 kg/m^3,
VOF-Fraktionen 0.25/0.75 an (2,2,2)/(5,5,5), Zellmittelpunkt-Vektoren
(2,-1,0.5) und (-1,0,-2) m/s:
Masse **1 kg**, Impuls **(-0.25,-0.25,-1.375) kg m/s**,
kinetische Energie **2.53125 J**, Gravitationspotenzial **4.65975 J**,
mechanische Summe **7.191 J**. Eine extrem schnelle trockene Rand-Face
wird aus dem Energie- und Impulskonto ausgeschlossen. Negative/zu grosse
VOF-Fraktionen, Solid-Ueberlappung, ungueltige Parameter und Rastergroesse
werden abgelehnt.

Zusaetzlich wird ein **laufender** verifizierter und nachprojizierter
MAC/VOF-Schritt mit 8^3 und 12^3 ausgefuehrt. Jeweils 2 kg Wasser,
nichtnegative endliche kinetische Energie und Paritaet zu den bestehenden
Wassermassen-/Gravitationsdiagnosen. Read-only-Wiederholung und Rollback
bei erzwungener Unterkonvergenz sind getestet.

Reproduktion:
`scripts/p2_vof_mechanical_inventory_full_build.cmd`
und `python scripts/p2_vof_mechanical_inventory_acceptance.py`.
Logs und SHA-256-Fingerprints:
`ENGINE TEST STUBE/runs/p2_vof_mechanical_inventory_*.txt` und
`ENGINE TEST STUBE/runs/p2_vof_mechanical_inventory_partial.json`.

## Nicht nachgewiesen

Zellmittelpunkt-Sampling erfasst weder die exakte VOF-Face-Verteilung
noch Druckarbeit, Grenzflaechen-/Viskositaetsarbeit, dissipierte Waerme
oder bewegliche Festkoerper-Gegenimpulse. Es gibt weiterhin keine
globale Fluid-/Festkoerper-Energieerhaltung, keine unabhaengigen
3D-CFD-Messdaten, keine neue GPU/LOD- oder UE5.8-Native-E2E-Abnahme.
Ein gruenes Teil-Gate schliesst P2 oder andere Vollphasen nicht ab.

## Erweiterung: Delta-Audit und VOF-Transportpotenzial (31.10.2026)

Die neue read-only-Funktion `vof_mechanical_delta_audit3d(before,after,
transport_gravitational_delta_j)` vergleicht zwei physikalisch gewichtete
Wasserinventare. Sie liefert Massendifferenz, drei Impulsdifferenzen,
Aenderungen der kinetischen und potenziellen Energie, die vom echten
VOF-Transport berichtete Potenzialverschiebung und deren algebraischen
Abschlussrest. `nontransport_mechanical_delta_j` ist definitionsgemaess
`(E_after-E_before)-Delta_E_transport` und **kein** nachgewiesener
Dissipations-, Druckarbeits- oder FSI-Erhaltungsterm.

Analytischer Zwei-Zellen-Fall (8^3 und 12^3): 0.125 kg Wasser wandern
um 0.3 m nach oben; bei fest vorgegebenen unterschiedlichen
Zellgeschwindigkeiten: Delta-Masse 0 kg, Delta-Impuls
(-0.375,+0.125,-0.3125) kg m/s, Delta-E_kin=-0.015625 J,
Delta-E_pot=+0.367875 J, Delta-E_mech=+0.35225 J. Eine absichtlich
falsche Transportbuchung erzeugt den erwarteten Rest +0.367875 J.
Nichtfinite Transportbuchungen und ungueltige Inventare werden abgelehnt.

Der echte `step_verified_reprojected` wurde auf 8^3 und 12^3
erneut ausgefuehrt. Die beiden unabhaengig aus VOF-Zellhoehen
berechneten Potenzialdifferenzen stimmen mit der Live-Transportbuchung
(-0.00962361 J) auf besser als 1e-10 J ueberein. Die
zellzentrierte kinetische Energie steigt um etwa +0.00911893 J;
der verbleibende mechanische Term ist damit **nicht null**.
Rollback und unveraenderte Inventare bleiben getestet.

MSVC Release: **94/94 CTest PASS**, **98 native Assertions**,
separates Python-Gate mit Quell-, Binaer- und Log-SHA256.
Es handelt sich um einen zusaetzlichen P2-Teilnachweis, nicht um
physikalische Energieerhaltung. **P2-P8 INCOMPLETE.**
