# P2 Teilabnahme: 3D-Wasserflussrekonstruktion im geschlossenen Becken (29.10.2026)

**Status: PARTIAL.** Dieser optionale Operator baut auf dem bereits getesteten
`P2ClosedBasinHydrostatic3D` auf. Er erzeugt erstmals ein **konservatives,
gerichtetes 3D-MAC-Face-Volumenflussfeld** zwischen dessen zwei
hydrostatischen Wasserinventaren bei vertikal bewegtem Starrkoerper.

## Numerische Definition

- Eingang: Anfangs-/End-Wasserfraktionen, echte alte/neue Cut-Cell-
  Volumina und zeitgemittelte offene MAC-Flaechen.
- Pro Zelle ist `b_i = V_i(before)-V_i(after)` der notwendige
  integrierte Nettoabfluss [m^3]. Ein geschlossener Beckenrand hat
  ausnahmslos `q_boundary=0`.
- Auf dem Graphen der zeitgemittelt offenen, waehrend des Schrittes
  zumindest teilweise benetzten Nachbarflaechen wird `L phi=b` mit
  konjugierten Gradienten geloest. `q_(i->j)=w_ij(phi_i-phi_j)`.
  Das minimiert die gewichtete quadratische Flussnorm bei vorgegebener
  diskreter Divergenz. Dies ist **kein** Navier-Stokes-Druckloeser.
- `w_ij = open_aperture * max(water_fraction_old/new, beide Zellen)`
  ist eine ausdruecklich kinematische Wahl, kein physikalisches
  Upwind-/Dichte-/Viskositaetsmodell. U/V/W sind integrierte
  Volumenfluesse [m^3], keine beschleunigten Fluidimpulse.
- Die Graph-Laplace-Matrix besitzt einen Nullraum **pro verbundener
  benetzter Komponente**. Globale Massenerhaltung allein reicht nicht.
  Eine Disjoint-Set-Pruefung erzwingt deshalb vor CG die unabhaengige
  Nullbilanz jeder Komponente, einschliesslich isolierter Trockenzellen.
  Unmoegliche Transfers zwischen getrennten Wasserinseln werden mit
  einer spezifischen Fehlermeldung abgewiesen. Erst dann wird ohne
  Druck-Gauge iteriert. Nicht konvergierende Faelle brechen ab.
- Die 3D-Zellbilanz wird *unabhaengig* aus U/V/W-Faces und den
  alten/neuen offenen Zellvolumina im nativen Test neu berechnet.

## Reproduzierbare Tests

- `scripts/p2_closed_basin_kinematic_flux_full_build.cmd`:
  MSVC-Release-CMake-Build und vollstaendiger CTest-Durchlauf.
- `scripts/p2_closed_basin_kinematic_flux_acceptance.py`:
  32 unabhaengig kontrollierte 3D-Konfigurationen (8^3/12^3,
  Wasserstand 0/3.2/4/5.75 m, g=0/9.81, v0=+/-0.4 m/s).
  Prueft Zell- und globale Massenbilanz, versiegelte Beckenraender,
  gesperrte MAC-Faces, Nullfluss bei trockenem Becken, Negativfaelle,
  CG-Konvergenz und Quell-/CTest-SHA256.
- Zusaetzlicher synthetischer 12x8x8-Test: zwei durch trockene
  Zwischenzellen getrennte Wasserinseln. Unabhaengig ausgeglichene
  Inseltransfers werden geloest; zwei global ausgeglichene, aber
  lokal unmoegliche Transfers (zwischen Inseln und in isolierte
  Trockenzellen) werden vor dem CG-Lauf abgelehnt.
- Nachweise liegen ausschliesslich unter
  `D:\Projekt Engine\ENGINE TEST STUBE\runs\` und werden nach
  jedem Neubau durch den Akzeptanzlauf aktualisiert.

## Physikalische Grenzen

Dies ist ein **kinematischer Rekonstruktionsoperator**, kein
zeitintegrierter 3D-Wassersolver. Es gibt weder Fluidtraegheit noch
VOF-Advektion aus dem berechneten Fluss; die Endfraktionen werden
hydrostatisch vorgegeben. Die Fluesse liefern keine Druckkraefte,
keine Wasser-Luft-Spannungsrandbedingungen und keine auf MAC
angewendete hydrostatische Festkoerper-Gegenreaktion. Die
Energiebilanz des quasistatischen Vorgängers ist **kein**
Nachweis dynamischer FSI-Energieerhaltung.

P2 bleibt PARTIAL. P3-P8 bleiben PARTIAL. Unreal-5.8-Native-E2E
fuer diesen neuen Pfad und unabhaengige gemessene 3D-CFD-Referenzen
fehlen. Naechster technischer Schritt: dynamische zeitdiskrete
MAC/VOF-Geschwindigkeits- und Impulsintegration auf Basis der
rekonstruierten Fluesse, mit gemeinsamer Wasser-/Koerperenergie.
