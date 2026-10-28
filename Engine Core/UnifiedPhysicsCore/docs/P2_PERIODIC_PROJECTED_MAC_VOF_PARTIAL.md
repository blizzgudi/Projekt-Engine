# P2 – Periodischer projizierter 3D-MAC/VOF-Transport (Teilnachweis)

Stand: 28.10.2026. **P2 ist nicht vollständig abgenommen.**

## Implementierung

Die optionale Header-only-Komponente `include/upc/p2_periodic_mac_vof3d.hpp`
führt `P2VariableMacTransport3D::self_advect`, die periodische
PCG-Druckprojektion und einen konservativen Upwind-Finite-Volume-Transport
der zellzentrierten Wasserfraktion in einem Zeitschritt zusammen.
Der Transport verwendet die **projizierten** MAC-Face-Geschwindigkeiten,
nicht die ungeprüften Prädiktoren. Er prüft periodische Nähte,
diskrete Divergenz, lokale ausgehende CFL, Fraktionsgrenzen [0,1],
endliche Werte und die diskrete globale Fraktionssumme.
Eingabefelder sind unveränderlich; Fehler führen zu einer Exception,
nicht zu einem teilweisen State-Update.

Die Fraktion ist ein **passiver, konstantdichter Marker**. Sie ist
kein physikalisch vollständiger Wasser/Luft-Zweiphasensolver. Insbesondere
fehlen Oberflächenspannung, Sprünge in Druck und Dichte, viskose
Spannungsbedingungen an einer echten freien Oberfläche, Festkörperkontakt
und dynamische bidirektionale Impulsrückwirkung.

## Reproduzierbare native Abnahme

`scripts/p2_periodic_mac_vof_full_build.cmd` initialisiert MSVC,
konfiguriert CMake, baut alle Targets und führt den vollständigen
CTest-Regressionslauf aus. Danach prüft
`python scripts/p2_periodic_mac_vof_acceptance.py` die nativen Ausgaben,
analytische Soll-Massen und CTest-Nachweise, schreibt Source-SHA-256,
CTest-Log-SHA-256 und Native-Stdout-SHA-256 in
`ENGINE TEST STUBE/runs/p2_periodic_mac_vof_partial.json`.

Ergebnisse des Laufs vom 28.10.2026:

- **67/67 CTest-Suites** erfolgreich (MSVC Release).
- **65.804 native Assertions** in der neuen Testsuite.
- Drei nichtkubische 3D-Gitter: 6×7×8, 8×9×10, 10×12×14.
- Positive und negative konstante Transportgeschwindigkeiten:
  **vier Zeitschritte** und zellweiser Vergleich mit unabhängig
  implementierter 3D-Upwind-Referenz, Toleranz 2e-12.
- Drei räumlich variable, diskret divergenzfreie Scherfelder:
  **vier nichtlineare MAC-Transport-/Projektions-/VOF-Schritte**,
  unabhängige zellweise Finite-Volume-Referenz, Toleranz 2e-12.
- Gesamtfraktionssumme auf jedem Gitter unverändert innerhalb 2e-10;
  maximale projizierte Divergenz kleiner 1e-10 s^-1;
  Fraktionsgrenzen eingehalten.
- Sieben Negativprüfungen: Fraktion außerhalb [0,1], NaN,
  negative Dichte, überschrittene CFL, fehlerhafte periodische Naht,
  divergentes Transportfeld und falsche Feldgröße. Keine Änderung
  der übergebenen Eingabedaten.
- Zusätzlich **21 unabhängige numerische Akzeptanzkontrollen**
  über neun Testfälle im maschinenlesbaren Teil-Gate.

## Offene Vollabnahme

Die neue Integration ist ausschließlich periodisch, ohne Cut-Cells und
ohne tatsächliche freie Wasseroberfläche. Es fehlen insbesondere
die Integration in `CoupledMacWater3D`, bewegte Festkörper mit
konservativer Fluid-Festkörper-Impuls- und Energiebilanz, unabhängige
3D-CFD-Messreferenzen und ein Unreal-5.8-Native-E2E für diesen Pfad.
P3–P8 bleiben ebenfalls offen. Das Report-Flag ist daher
`partial_accepted=true`, aber **`phase_complete=false`**.
