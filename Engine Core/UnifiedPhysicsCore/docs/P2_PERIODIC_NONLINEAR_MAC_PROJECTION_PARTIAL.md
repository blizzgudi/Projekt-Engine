# P2 – Periodische 3D-MAC-Impulsadvektion mit Druckprojektion (Teilnachweis)

Stand: 28.10.2026. **P2 bleibt PARTIAL; P3–P8 ebenfalls.**

## Implementierung

- Neuer optionaler, eigenständiger `P2PeriodicMacProjection3D` in
  `include/upc/p2_periodic_mac_projection3d.hpp`.
- Der vorhandene konservative `P2VariableMacTransport3D::self_advect`
  erzeugt auf dem periodischen, vollständig gefüllten, konstantdichten
  3D-MAC-Gitter den nichtlinearen Impuls-Prädiktor.
- Matrixfreier 7-Punkt-Operator `A=-L` und Jacobi-vorkonditioniertes
  Conjugate Gradient lösen `A p=-(rho/dt) div(u*)`. Der periodische
  Nullraum wird über Mittelwertabzug und Druck-Gauge 0 behandelt.
- Korrektur `u_new=u*-(dt/rho) grad(p)` auf gestaffelten Faces;
  redundante periodische High-Seams werden gespiegelt. Ein Fehlschlag
  wirft eine Exception, ohne Eingaben oder vorherige Zustände zu ändern.
- `self_advect_project` koppelt erstmals den ortsvariablen konservativen
  nichtlinearen MAC-Transport mit anschließender Druckprojektion über
  mehrere Schritte. **Dies ist kein vollständiger Navier-Stokes-Solver.**

## Reproduzierbare Prüfung

`scripts/p2_periodic_projection_full_build.cmd` baut den gesamten
MSVC-Release-Core und schreibt den vollständigen CTest-Nachweis nach
`ENGINE TEST STUBE/runs/p2_periodic_projection_full_ctest_20261028.txt`.
`scripts/p2_periodic_projection_acceptance.py` überprüft native
Ausgaben gegen unabhängig berechnete analytische Fourier-Druckreferenzen,
Prüfungsanzahlen, CTest-Protokoll und SHA-256-Fingerprints.
Der JSON-Bericht liegt in
`ENGINE TEST STUBE/runs/p2_periodic_projection_partial.json`.
Alle Prüfnachweise sind nur bei tatsächlich erfolgreichem Lauf gültig.

Die drei nichtkubischen Gitter 8x10x12, 12x14x16 und 16x18x20
prüfen analytische periodische Druckfelder
`p=100 cos(2pi i/nx)+40 cos(2pi j/ny)+20 cos(2pi k/nz)`,
Druck-Gauge, Entfernung der Gradientenkomponente, Divergenzreduktion,
vier gekoppelte nichtlineare Schritte, periodische Komponentensummen,
Nullprojektion, CFL und negative Eingaben.

## Strikte Grenzen

- Periodische, vollständig gefüllte, inkompressible **konstantdichte**
  MAC-Kontrollvolumina; kein Druck-Ghost-Fluid und keine freie Oberfläche.
- Keine bewegten Cut-Cells, keine Kopplung an `CoupledMacWater3D` oder
  `P2PrescribedBoxMacVof3D`, kein Fluid-Boden-Kontakt und keine
  bidirektionale Festkörperreaktion.
- Komponentenintegrale sind **diskrete Proxy-Impulssummen**, kein
  Nachweis globaler physikalischer Impuls- oder Energieerhaltung.
- Analytische Manufactured-Lösungen sind keine unabhängigen
  3D-CFD-Messdaten. Keine neue Unreal-5.8-Native-E2E-Abnahme.
- Kein GPU/LOD-, Wärme-, Festkörper- oder Reaktions-Vollphasengate.

Die nächste P2-Aufgabe ist die Übertragung der nichtlinearen
Prädiktor-Druckkorrektur auf freie Oberflächen und bewegte Cut-Cells
mit tatsächlicher Flüssigkeits-/Festkörper-Impuls- und Energiebilanz.
