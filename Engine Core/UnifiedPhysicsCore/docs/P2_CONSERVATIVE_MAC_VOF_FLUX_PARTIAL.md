# P2 Teilnachweis: konservativer 3D-MAC-Face-Flux für VOF

Stand: 2026-10-27. **P2 bleibt PARTIAL. Keine Vollphasenabnahme.**

## Implementierung und Gültigkeitsbereich

`include/upc/p2_conservative_vof_flux3d.hpp` implementiert einen
optionalen, isolierten, expliziten, konservativen Finite-Volume-Operator.
Er transportiert Wasserfüllungen (VOF in [0,1]) über echte, lokal
benachbarte MAC-Faces. Der Fluss pro Face ist
`(dt/dx) * |u_face| * fill_donor`; derselbe Betrag wird dem Donor
abgezogen und dem Empfänger zugeführt. Es gibt **keinen**
augenblicklichen Wassertransfer durch einen Starrkörper wie bei den
vorherigen topologischen Sweep-Prototypen.

Eingaben: drei 3D-MAC-Face-Velocity-Arrays, feste Solid-Maske, VOF,
`dx`, `dt`. Die Methode verlangt einen diskret divergenzfreien
Geschwindigkeitszustand, exakt undurchlässige äußere und feste
Solid-Grenzen und eine Summe aller ausgehenden Courant-Zahlen pro Zelle
von höchstens 1. Ungültige Eingaben werden vor Rückgabe verworfen.
Eingaben werden niemals mutiert. Für inkompressible Flüsse gilt die
konservative und beschränkte erste Ordnung der Donor-Cell-Advektion.

## Analytische und numerische Referenzen

- Unabhängige, vierzellige, geschlossene Face-Zirkulation:
  Ausgangsfüllung (1,0,0,0), `u*dt/dx=0.25`.
  Nach einem Schritt (0.75,0.25,0,0); nach zwei Schritten
  (0.5625,0.375,0.0625,0). Die Richtungsumkehr und alle
  drei 3D-Achsenebenen XY/XZ/YZ werden bei 8³, 12³ und 16³ getestet.
- Diskrete, durch eine Vertex-Stromfunktion definierte 3D-Face-Strömung:
  `u = delta_y(psi)`, `v = -delta_x(psi)`, `w=0` auf
  mehreren Z-Ebenen; Divergenz verschwindet diskret.
  Feste Hindernisse bleiben trocken, VOF bleibt beschränkt,
  Wasserbilanz bleibt erhalten und die Wasserverteilung verändert sich.
- Negativtests: CFL-Verletzung, kompressibles Face-Feld,
  Fluss durch Außenwand oder Solid, ungültiges VOF, NaN,
  ungültige Arraygröße und Null-Zeitschritt.
- Reproduzierbar: `build/upc_p2_conservative_vof_flux_tests.exe`,
  `scripts/p2_conservative_vof_flux_acceptance.py`, die volle
  CTest-Suite und SHA-256-Fingerprints der geprüften Quellen.

Nachweise: `ENGINE TEST STUBE/runs/p2_conservative_vof_flux_partial.json`
und `ENGINE TEST STUBE/runs/p2_conservative_vof_flux_full_ctest_20261027.txt`.

## Klare Grenzen

Dies ist **noch keine dynamische Fluid-Festkörper-Kopplung**. Die
Solid-Maske ist statisch, es gibt keine bewegte Randgeometrie,
keine Rückübertragung von Impuls und Energie auf den Körper,
keine zeitaufgelöste gesweepte Volumenkonservierung, keinen
druckkorrigierten Gesamt-FSI-Schritt und keine realen 3D-CFD-Messdaten.
Die numerische Donor-Cell-Advektion ist diffusionsbehaftet.
Die Komponente ist bewusst opt-in und nicht automatisch in den
produktiven `CoupledMacWater3D`-Solver eingebunden.
Ein neuer Unreal-5.8-Native-E2E-Test für diesen Operator fehlt.

**Nächste Aufgabe:** Einen wirklich zeitaufgelösten beweglichen
Solid-Grenzfluss mit diskreter geometrischer Volumenbilanz und
MAC-Impulsrückwirkung entwickeln, unabhängig validieren und danach
erst in den Live-Solver integrieren.
