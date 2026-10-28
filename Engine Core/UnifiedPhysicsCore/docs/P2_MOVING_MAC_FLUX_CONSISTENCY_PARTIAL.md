# P2 Teilabnahme: Flux-Konsistenz des bewegten MAC-Primal-/Dual-Pfads

Stand: 28.10.2026. **P2 PARTIAL**, P3â€“P8 ebenfalls offen.

## Umfang
`P2MovingMacFluxConsistency3D::audit` projiziert zunÃ¤chst **ein** gemeinsames
primitives MAC-Feld gegen die bewegte Cut-Cell-GCL. Die drei daraus
rekonstruierten Dualgitter werden anschlieÃŸend weiterhin separat projiziert.
Der neue Audit berechnet fÃ¼r jedes Dualgitter und jede Face-Komponente

`Q_pred = dt * dx^2 * aperture * u_interpoliert`

und vergleicht dies mit dem tatsÃ¤chlich druckkorrigierten Dualfluss
`Q_dual`. Gemessen werden `sum(abs(Q_dual-Q_pred))`, das Maximum,
die Zerlegung nach drei Dual-Komponenten sowie die beiden GCL-Residuen.
Die MaÃŸeinheit der Flussdifferenzen ist mÂ³ **pro Zeitschritt**. Dies ist
keine ImpulsgrÃ¶ÃŸe und kein Nachweis einer konsistenten physikalischen
Impulstensor-Diskretisierung.

## Reproduzierbare Nachweise
- `scripts/p2_moving_mac_flux_consistency_full_build.cmd`: MSVC Release,
  vollstÃ¤ndiger CTest einschlieÃŸlich neuer Suite.
- `scripts/p2_moving_mac_flux_consistency_acceptance.py`: native
  AusfÃ¼hrung, quantitative Auswertung, Quell-SHA256 und CTest-Log-SHA256.
- `ENGINE TEST STUBE/runs/p2_moving_mac_flux_consistency_full_ctest_20261028.txt`
  und `ENGINE TEST STUBE/runs/p2_moving_mac_flux_consistency_partial.json`.
- Zwei nichtkubisch **nicht** variierte Gitter (8Â³ und 12Â³), alle drei
  Bewegungsachsen und beide Richtungen = 12 bewegte FÃ¤lle, zusÃ¤tzlich
  zwei stationÃ¤re Nullreferenzen und NegativfÃ¤lle.
- Test berechnet unabhÃ¤ngig die tatsÃ¤chlichen neun Face-Flussdifferenzen
  je Konfiguration und vergleicht Summe, Komponenten und Maximum.
  Das Nullfeld bei stationÃ¤rem KÃ¶rper erfordert exakt Nullkorrektur.
- Ein erster Testlauf scheiterte an einer **falschen Testannahme**:
  Ein Null-Primal-PrÃ¤diktor bleibt nach der GCL-Projektion eines bewegten
  KÃ¶rpers nicht Null. Die Annahme wurde korrigiert; der erfolgreiche
  vollstÃ¤ndige Folgelauf ist alleiniger Abnahmenachweis.

## Interpretation
Die getrennten Dual-Projektionen erzeugen messbare Korrekturen auch nach
gemeinsamer Primal-Projektion. Diese Diagnose macht die verbleibende
Diskretisierungsinkonsistenz quantitativ sichtbar. Sie korrigiert sie nicht.
Es fehlen gemeinsame tensorielle ImpulsflÃ¼sse, bidirektionale
FestkÃ¶rper-Gegenreaktion, physikalische Gesamtenergie, freie
OberflÃ¤chenspannungsbedingungen, externe 3D-CFD-Messdaten und der neue
UE5.8-Native-E2E-Pfad. Bestehende Live-Solver wurden nicht verÃ¤ndert.

