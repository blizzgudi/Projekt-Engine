# P2 Teilnachweis: gemeinsamer 3D-MAC-Prädiktor auf bewegten Dualgittern

Stand: 28.10.2026. **P2 bleibt PARTIAL; keine Vollphasenabnahme.**

## Implementierung
`P2MovingMacSharedPredictor3D::reconstruct` übernimmt EIN gemeinsames
primitives, gestaffeltes 3D-MAC-Geschwindigkeitsfeld (u/v/w). Für jedes der
drei um eine halbe Zelle verschobenen MAC-Dualgitter werden alle drei
Transportkomponenten mit dem exakt definierten arithmetischen
Halbzellen-Mittel zweier benachbarter Primal-Faces rekonstruiert.
Äußere und durch die bewegte Quadergeometrie geschlossene Dual-Faces
erhalten Null. Nichtfinite, falsch dimensionierte oder normal nach außen
strömende Primal-Faces werden verworfen.

`step` nutzt diesen gemeinsamen Prädiktor in drei
`sample_with_predictor`-Druckprojektionen, anschließend den bereits
vorhandenen konservativen signierten Dualvolumen-Upwind-Transport.
Exakte bewegte Dualgeometrie, GCL, lokale Donor-CFL und
Komponentenintegration werden geprüft. Bei Fehlern bleiben sämtliche
Eingabefelder unverändert. Der bestehende `CoupledMacWater3D`-Pfad
wird nicht geändert.

## Validierungsumfang
- Drei 3D-Gitter (8³/12³/16³), alle drei Bewegungsachsen und beide
  Richtungen, jeweils zwei aufeinanderfolgende Zeitschritte.
- Analytische Halbzellen-Mittelwerte an jedem offenen Dual-Face
  und Nullwerte auf blockierten bzw. äußeren Faces.
- Unabhängige integrale Bilanz vor/nach jedem Transport,
  GCL und CFL, signierte Komponenten.
- Null-Prädiktor reproduziert den vorherigen Operator exakt;
  nichttrivialer gemeinsamer MAC-Prädiktor verändert die Advektion.
- Fehlerfälle: ungültige Dimensionen, NaN, normaler Außenfluss,
  fehlerhafte Dualwerte und Dichte; transaktionale Eingaben.
- Reproduzierbar mit `scripts/p2_moving_mac_shared_predictor_full_build.cmd`
  und `scripts/p2_moving_mac_shared_predictor_acceptance.py`.
  CTest-Protokoll und JSON-Teilgate liegen ausschließlich unter
  `D:\Projekt Engine\ENGINE TEST STUBE\runs`.

## Grenzen
Der Prädiktor stammt jetzt aus EINEM primalen MAC-Feld, **aber die
drei Dual-Projektionen sind weiterhin unabhängig**. Die
druckkorrigierten Dualflüsse sind daher kein einheitlicher
diskreter Impulstensor und keine vollständige physikalische
nichtlineare Navier-Stokes-Selbstadvektion. Das Modell besitzt
keine freie Oberflächen-Spannungsrandbedingung, dynamische
Fluid-Festkörper-Gegenreaktion, variable Dichte, global
geschlossene mechanische Energie oder extern gemessene
3D-CFD-Validierung. Keine Integration in den Live-Solver oder
UE5.8 Native-E2E. P3–P8 bleiben ebenfalls PARTIAL.
