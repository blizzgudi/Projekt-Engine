# P2 Teilmeilenstein — massenkonsistenter 3D-Zweiphasen-Impulstransport

**Stand: 02.11.2026. P2 weiterhin PARTIAL; keine Vollabnahme.**

## Technische Änderung

Neue getrennte, opt-in Module:

- `include/upc/p2_two_phase_consistent_momentum3d.hpp`
- `include/upc/p2_two_phase_momentum_vof3d.hpp`

Der erste Baustein transportiert Wasser- und Luftmasse sowie die drei
kartesischen Komponenten des **zellzentrierten Impulses** mit exakt
demselben zeitintegrierten, projizierten 3D-MAC-Face-Fluss. An jeder
Zellgrenze wird die Dichte des Upwind-Donors verwendet:

`m_face = |u_face| · dt / dx · (rho_L α_donor + rho_G (1-α_donor))`.

Der Impulsfluss ist `m_face · v_cell,donor`, mit exakt
gegengleichen Vorzeichen in beiden Nachbarzellen. Die unabhängige
Voxel-VOF-Advektion muss denselben Wasserfüllgrad und dieselbe
Mischungsdichte liefern. Der Operator prüft sechsseitigen CFL,
diskrete Divergenz, physikalische Dichte, separate Wasser-/Luftmassen,
globale Summe des zellzentrierten Impulses und die nichtnegative
Upwind-Kinetikdissipation. Er verschleiert keine numerischen
Bilanzfehler durch beliebige Massenkorrekturen.

Der zweite Baustein wird ausdrücklich getrennt von der bisherigen
Standard-API aufgerufen. Er führt den bereits bestehenden
variabel-dichten Zweiphasen-MAC-PCG-/Schwerkraft-/VOF-Schritt aus,
transportiert die zellzentrierten Geschwindigkeiten mit dem
**tatsächlich projizierten** Face-Fluss und übergibt die geänderte
Geschwindigkeit durch Interpolation an die MAC-Faces. Anschließend
korrigiert eine zweite reale variabel-dichte PCG-Projektion die
durch die Übertragung neu entstandene Divergenz.

Somit wird die Impulsadvektion **wirklich in die neue
MAC-Geschwindigkeitsentwicklung zurückgeführt**. Bestehende
Funktionen und Defaults bleiben unverändert. Der gesamte neue
Schritt arbeitet transaktional: Fehler führen nicht zu einem
teilweise übernommenen Fluidzustand.

## Native Nachweise

`tests/p2_two_phase_consistent_momentum3d_tests.cpp`:
vier 3D-Fälle mit 4×4×4, 6×5×4, 8×6×5, 12×8×6 und jeweils
sieben Transportzeitschritten. Die unabhängige Zell-Divergenzformel
führt alle Flüsse erneut aus (ohne Aufruf des Produktions-VOF-Sweeps).
Die analytische Konstanz einer uniformen dreikomponentigen
Geschwindigkeit bei bewegter Dichteverteilung besteht ebenso wie
die Dissipation eines räumlich variierenden Impulsfeldes.

39.191 native Assertions waren im separaten Win64-MSVC-Lauf erfolgreich;
maximaler Dichte-/Referenzfehler ca. 4,55 × 10⁻¹³
und maximaler 3D-Impulssummenfehler 3,91 × 10⁻¹⁴ kg m/s.

`tests/p2_two_phase_momentum_vof3d_tests.cpp`:
echter gekoppelter P2-Zweiphasenschritt auf
4×4×4 / 6×5×4 / 8×6×5, je sechs Schritte mit schrägem
Wasser-Luft-Füllgrad, dreikomponentiger Strömung,
Schwerkraft, variabler Dichte und zwei echten PCG-Projektionen.
Die Resultate wurden gegen den älteren Druck-/VOF-Modus ohne
Impulsfeedback verglichen.

5.337 native Assertions waren im separaten MSVC-Lauf erfolgreich.
Die tatsächlich rückgekoppelten MAC-Faces unterscheiden sich nach
sechs Schritten um maximal 0,08710 / 0,08478 / 0,06238 m/s.
Die zellzentrierten internen Impulsflüsse schließen bis ca.
1,67 × 10⁻¹⁶ / 3,33 × 10⁻¹⁶ / 1,11 × 10⁻¹⁵ kg m/s.
Fehlgeschlagene PCG- und Wand-/Gravitationsprüfungen
ändern den Eingangsfluidzustand nicht.

## Fehlender physikalischer Gesamtimpulsschluss — ausdrücklich FAIL

Die anschließende **zellzentriert-zu-staggered-MAC-Interpolation**
ist noch **keine konservative duale MAC-Kontrollvolumen-Übertragung**.
Trotz perfekter Zell-Fluss-Impulssumme bleibt die
Remap-plus-PCG-Impulsänderung sichtbar:

| 3D-Gitter | maximale nicht geschlossene MAC-Impulsänderung |
|---|---:|
| 4×4×4 | 0,054522 kg m/s |
| 6×5×4 | 0,080531 kg m/s |
| 8×6×5 | 0,085655 kg m/s |

Diese Werte sind im Live-Resultat
`remap_plus_feedback_impulse_change_kg_m_s` dokumentiert.
Der Flag `cell_to_mac_is_conservative=false` darf nicht
als Abnahme übergangen werden. Es fehlen eine
massenerhaltende duale MAC-Impulsadvektion, eine konsistente
bewegte Wasser-Luft-Grenzspannung, viskose/capillare
Gesamtkräfte, dynamische bidirektionale Fluid-Festkörper-Wechselwirkung,
die erfolgreiche MARIN/SPHERIC 6-s-Dreigitter-Messabnahme und
ein neues UE5.8-Native-E2E für diese Kette.

**P2 bleibt INCOMPLETE; ALL_COMPLETE=False.**

Reproduktion mit `scripts/p2_two_phase_momentum_build.cmd`
und unabhängige Teilabnahme mit
`scripts/p2_two_phase_momentum_acceptance.py`. Testlogs
und signierte Source-/Build-/Native-Hashes im vorhandenen Ordner
`ENGINE TEST STUBE/runs`.
