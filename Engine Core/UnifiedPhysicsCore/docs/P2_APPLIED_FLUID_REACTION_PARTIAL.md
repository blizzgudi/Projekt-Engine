# P2 Teilnachweis: tatsächlicher Gegenimpuls in 3D-MAC-Fluid-Faces

Stand: 27.10.2026. **P2 PARTIAL. Keine vollständige bidirektionale FSI-Abnahme.**

## Neu implementiert

`include/upc/pressure_reaction_transfer3d.hpp` ergänzt den bisherigen
`RigidPressureResponse3D`-Schritt (Druckkraft → Körperimpuls) um eine
**tatsächliche Änderung des Fluidgeschwindigkeitsfelds**. Der auf den Körper
wirkende lineare Druckimpuls `J_body=F*dt` erzeugt die explizite
Gegenreaktion `J_fluid=-J_body`. Diese wird für jede der drei MAC-Komponenten
gleichmäßig auf *interne wet-wet-Faces* verteilt, deren benachbarte
Flüssigkeitszelle eine Fläche mit dem ausgewählten Festkörper teilt.
Wandnormalen, äußere Ränder, Luft-Faces und feste Voxel bleiben unverändert.

Der diskrete, **nicht physikalisch integrierte** Face-Impuls verwendet
`m_face=rho*dx^3`, `delta_v_a=J_fluid_a/(N_a*m_face)`. Die Funktion
gibt die tatsächliche Face-Impulssumme, Anzahl der berührten Faces,
Änderung des quadratischen Face-Energie-Proxys und den **nicht**
übertragenen Drehimpuls zurück. Sie prüft Eingaben, Masken,
numerische Überläufe und alle Updates, **bevor** sie das MAC-Feld verändert.

Dies ist eine erste explizite Rückwirkung auf Fluidgeschwindigkeiten,
nicht nur ein buchhalterisches negatives Vorzeichen. Die Körperposition
aus `RigidPressureResponse3D` wird **nicht** auf die Festkörpermaske
zurückgeschrieben; Geometrie und Drucklast bleiben während des Schritts
eingefroren.

## Tatsächlicher Testumfang

`tests/p2_pressure_reaction_transfer3d_tests.cpp` prüft 12 echte
3D-MAC-Projektionen: drei Gittergrößen (8, 12, 16), offene
Wasser-Luft- bzw. geschlossene Neumann-Ränder und zwei Druckoffsets
(150/1200 Pa). Jeweils wird eine benetzte 2×2×2-Voxelkörpermaske
mit 24 Außenflächen verwendet. Die unabhängig analytisch erwartete
Kraft ist (-0.5376, +0.8448, -0.3840) N; bei dt=0.05 s
ergibt sich der Körperimpuls (-0.02688, +0.04224, -0.01920) Ns.
Der Fluid-Gegenimpuls muss auf den tatsächlich veränderten Faces
(+0.02688, -0.04224, +0.01920) Ns ergeben.

Das Testprogramm liest jede MAC-Face-Geschwindigkeit vor/nach dem
Transfer, prüft unveränderte unerlaubte Faces und summiert die
wirklich angewandten Änderungen unabhängig von der Implementierung.
Zusätzlich: NaN/Inf, ungültige Überlappungsmasken, Mass-Overflow,
Nullimpuls, reale Nachprojektion und verbleibende Divergenz.
Das separate Python-Gate berechnet die Referenzwerte unabhängig
vom C++-CSV, verifiziert 12 Szenarien und die vollständige CTest-Suite.

## Grenzen und fehlende Abnahmen

- **Keine bewegte Fluid-Festkörper-Grenze.** Körpertranslation wird
  noch nicht in eine neue Geometrie-/VOF-/Solidmaske übertragen.
- Die Gleichverteilung auf wet-wet-Faces ist ein lokaler
  **numerischer Face-Momentum-Proxy**, keine aus Druck- und
  Viskositätstensor abgeleitete konservative Finite-Volume-FSI-Flussform.
- Die spätere MAC-Druckprojektion verändert die Face-Impulse erneut.
  Das hier getestete Nullresiduum **vor** der Projektion ist deshalb
  keine globale Impulserhaltung des gesamten Zeitschritts.
- Das Gegendrehmoment wird nur als `angular_impulse_unapplied_Nms`
  ausgegeben. Kein Spin-/Torque-Transfer zum Fluid.
- Keine Energieerhaltung für Fluid+Festkörper, keine
  Oberflächenspannung, kein Cut-Cell-Mesh, keine extern gemessene
  3D-Strömungsreferenz, keine neue UE5.8-Game/PIE-Native-E2E.
- **P2–P8 bleiben offen.** Der Prototyp darf nicht als vollständige
  FSI- oder AAA-Engine-Abnahme ausgegeben werden.

## Nachweise

- C++-Test: `tests/p2_pressure_reaction_transfer3d_tests.cpp`.
- Separates Gate: `scripts/p2_applied_fluid_reaction_acceptance.py`.
- Maschinenlesbar: `D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_applied_fluid_reaction_partial.json`.
- Vollständiges CTest-Protokoll:
  `D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_applied_fluid_reaction_ctest_20261027.txt`.
- Alle nachweisrelevanten Quelldateien, das ausführbare Native-Binary
  und das CTest-Protokoll werden im Report mit SHA256 festgehalten.
