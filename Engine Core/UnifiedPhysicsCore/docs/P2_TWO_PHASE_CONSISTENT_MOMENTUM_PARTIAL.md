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


## Ergänzung 02.11.2026: massengewichtete 3D-MAC-Dualzell-Rekonstruktion

Die bisherige Live-Rückübertragung verwendete das arithmetische Mittel
benachbarter Zellgeschwindigkeiten, unabhängig von unterschiedlichen
Wasser-/Luft-Dichten. Dies ist kein massenkonsistentes MAC-Mittel.

Neues C++17-Modul: include/upc/p2_two_phase_mac_momentum_remap3d.hpp.
Für jedes der drei MAC-Richtungsfelder gilt die diskrete interne
Face-Masse M_f = 0,5 * (rho_L + rho_R) * ΔV und die Geschwindigkeit
u_f = (rho_L * u_L + rho_R * u_R)/(rho_L + rho_R).

Eine unabhängig überprüfte globale **Halbzell-Aufteilungsidentität**
erfasst nun exakt:

  Zellgesamtimpuls = Summe innerer MAC-Face-Impulse
                     + Summe virtueller äußerer Halbface-Impulse.

Die virtuellen Außen-Halbfaces sind ausschließlich
**geometrische Kontrollvolumenbuchungen**. Echte MAC-Wandnormalflüsse
bleiben null. Diese virtuellen Impulse werden nicht als physikalische
Wanddruckkräfte ausgegeben. Die zugehörige massengewichtete
Jensen-Energiedifferenz ist bei inhomogener Geschwindigkeit
nichtnegativ. Gleichförmige Geschwindigkeiten bleiben unverändert.

Der gekoppelte echte Zweiphasen-Schritt in
include/upc/p2_two_phase_momentum_vof3d.hpp
benutzt die neue massengewichtete Zell-zu-MAC-Rekonstruktion
wirklich vor seiner zweiten variablen-Dichte-PCG-Druckprojektion.
Das Druck-Wandimpulsledger wird weiterhin getrennt geprüft und
bei numerischer Abweichung verworfen. Alte Default-APIs bleiben erhalten.

### Native Nachweise

- Neuer unabhängiger 3D-C++-Test auf 4x4x4, 6x5x4, 8x6x5,
  12x8x6, alle drei Face-Richtungen, heterogene Dichten,
  konstanten/variablen Zellgeschwindigkeiten, Null-Wandfluss,
  genaue Halbzellimpulse, Energieungleichung und negative
  NaN-/Größen-/Dichte-Gates: **3.543 native Assertions PASS**.
- Der gekoppelte 3D-VOF/MAC/PCG-Live-Test verifiziert die neue
  Rekonstruktion, Druck-Wandimpuls- und Energiebilanz
  in sechs echten Updates je Raster: **5.691 native Assertions PASS**.
- Die bestehenden konservativen Zell-Impuls-FV-Tests bleiben bei
  **39.191 Native-Assertions PASS**.
- MSVC-Release 141/141 CTest-Suites PASS;
  unabhängige QA verwirft 17 manipulierte native Datenverläufe.
  Source-/Binary-/CTest-/Native-/Phase-Gate-Fingerprints:
  ENGINE TEST STUBE/runs/p2_two_phase_momentum_partial.json.

### Nicht geschlossene physikalische Gesamtbilanz

Die tatsächliche Live-MAC-Impulsänderung durch Rückübertragung und
Druckreaktion bleibt nicht null. Beobachtete maximale Beträge
(jeweils Maxima einzelner Komponenten/Zeitstufen, nicht skalare Summen):

| Raster | Remap + Druck | Remap allein | Druck-Wandreaktion |
|---|---:|---:|---:|
| 4x4x4 | 0,051102 kg m/s | 0,048266 kg m/s | 0,030566 kg m/s |
| 6x5x4 | 0,072655 kg m/s | 0,049011 kg m/s | 0,037812 kg m/s |
| 8x6x5 | 0,087295 kg m/s | 0,052672 kg m/s | 0,043954 kg m/s |

Diese Zahl ist kein impliziter Beleg für einen fehlenden
physikalischen Wandimpuls. Eine echte globale FSI-Impulsbilanz
muss Rand-/Halbzellkontrolle, physikalische Wände und
bewegte Festkörper gemeinsam erfassen. Die verbesserte lokale
Diskretisierung ist kein vollständiger P2-Phasenabschluss.

**P2 bleibt INCOMPLETE / ALL_COMPLETE=False.**
Weitere offene Kriterien: echte bewegte Grenzflächenspannungen,
voller MARIN/SPHERIC >=6-s-Dreigitter-Messvergleich mit H1-H4/P1-P8,
bidirektionale Fluid-Festkörperphysik, Gesamtenergie/-impuls und
aktueller UE5.8-Native-E2E für den neuen Zweiphasenpfad.
