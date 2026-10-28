# P2 Teilnachweis: konservativer Transport auf bewegten MAC-Dualvolumina

Stand: 28.10.2026. **P2 bleibt PARTIAL; keine Vollabnahme.**

## Umsetzung
`P2MovingMacDualTransport3D::step` verwendet die exakte, zeitlich bewegte
Dualvolumengeometrie für alle drei gestaffelten MAC-Komponenten. Für jede
Komponente wird auf dem entsprechend um eine halbe Zelle verschobenen
Dualgitter eine separate, geschlossene Cut-Cell-Druckprojektion mit
Null-Prädiktor berechnet. Die resultierenden integrierten Fluidvolumenflüsse
transportieren die signierten Komponentenwerte mit konservativem Upwind
einschließlich lokaler GCL- und Donor-CFL-Prüfung. Der Aufruferzustand wird
auch bei Fehlern nicht verändert. Es wird keine neue Engine-Schnittstelle
im allgemeinen `CoupledMacWater3D` aktiviert.

**Physikalische Grenze:** Drei unabhängig projizierte Dualflüsse bilden
noch keinen konsistenten nichtlinearen MAC-Impulstensor und insbesondere
keine Selbstadvektion durch die tatsächliche MAC-Strömung. Die getrennten
Komponentenintegrale sind numerische konservierte Größen, keine globale
Fluid-Festkörper-Impulsbilanz. Der Null-Prädiktor erzeugt lediglich
bewegungsbedingte Fluidvolumenflüsse. Kein dynamischer Festkörperimpuls,
keine Spannungsrandbedingung oder freie Wasseroberfläche.

## Reproduzierbarkeit
- `scripts/p2_moving_mac_dual_transport_full_build.cmd`: MSVC Release,
  gesamter CTest-Satz und vollständiges CTest-Protokoll.
- `scripts/p2_moving_mac_dual_transport_acceptance.py`: separate native
  Ausführung, unabhängige globale Dualvolumen-Referenz für konstante
  Werte, konservative signierte Komponentenintegration über mehrere
  Zeitschritte, Quell-SHA256 und CTest-Log-Hash.
- Belege: `ENGINE TEST STUBE/runs/p2_moving_mac_dual_transport_partial.json`
  und `p2_moving_mac_dual_transport_full_ctest_20261028.txt`.

## Noch offen
Konsistente duale Impulsflüsse aus einem einzigen MAC-Geschwindigkeitsfeld,
nichtlineare Advektion mit bewegten Wandflächen, dynamische Gegenreaktion
und thermomechanische Energieerhaltung, stresskonsistente freie Oberfläche,
unabhängige 3D-CFD-Referenzen, Integration in `CoupledMacWater3D` und
Unreal-5.8-Native-E2E. P3–P8 bleiben ebenfalls PARTIAL.

## Verifizierte Ausführung (28.10.2026)
- MSVC Release und gesamter CTest: **72/72 PASS**.
- Neuer nativer Test: **459472 Assertions**.
- Eigenständiges Python-Teilgate: **447 Checks**, darunter **216**
  zweistufige Gitter-/Bewegungs-/Komponentenfälle und **9** unabhängige
  analytische Referenzen für konstante signierte Dualkomponenten.
- Der maximale GCL-Fehler der protokollierten Fälle liegt unter 1e-12
  (dimensionslos); alle Donor-CFL-Werte liegen unter 1.
- P1-Referenzabnahme nach CMake-Änderung erneut **33/33 PASS**.
- P2 ist damit ausdrücklich NICHT voll abgenommen.
