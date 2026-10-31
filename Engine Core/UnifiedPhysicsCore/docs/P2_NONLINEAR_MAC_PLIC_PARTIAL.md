# P2 Teilabnahme: nichtlinearer periodischer 3D-MAC-Praediktor mit geometrischem PLIC (31.10.2026)

Die neue Opt-in-Funktion `p2_nonlinear_mac_plic_step3d` verbindet
erstmals innerhalb **dieses periodischen PLIC-Adapters** einen
zeitlich fortgeschriebenen Geschwindigkeitszustand mit dem geometrischen
VOF-Transport. Sie verwendet die vorhandene konservative
Upwind-Advektion der drei staggered MAC-Komponenten, anschliessend die
periodische konstante-Dichte-PCG-Druckprojektion und dann die
CFL-adaptive geometrische PLIC-Advektion der Wasserfuellgrade.
Der korrigierte MAC-Zustand und der Wasserzustand werden gemeinsam
zurueckgegeben und koennen im naechsten Schritt wieder eingespeist
werden. Bei einer Exception bleiben alle Eingabedaten unveraendert.

**Unabhaengige analytische Referenz:** periodische, vollstaendig in
zwei Querrichtungen ausgedehnte Wasserschicht mit drei vollen
Zellspalten. Bei konstanter Geschwindigkeit von +/-0,1 m/s und
dt=0,04 s, dx=0,1 m ist die Verschiebung exakt 0,04 Zellen je
Schritt. Nach drei Schritten liegen die analytischen Randfuellgrade
bei 0,88 und 0,12. Alle drei Achsen, beide Vorzeichen und
8^3/12^3 Raster werden pruefbar durchlaufen.

**Nichtlinearer Test:** 8^3 und 12^3, dreiachsige periodische
Querscherprofile, drei fortgeschriebene Schritte. Der
MAC-Impulspraediktor erzeugt eine messbare Divergenz, die PCG-
Druckprojektion entfernt sie; der aktualisierte MAC-Zustand
transportiert eine 3D-Wasserschicht geometrisch. Gegen eine
manuell zusammengesetzte Projektions-/PLIC-Referenz werden
Zellfuellgrade, Massenbilanz, periodische Komponentensummen und
Grenzwerte kontrolliert. Die manuelle Zusammensetzung ist ein
**Integrations-Paritaetstest**, keine externe CFD-Referenz.
Negativtests pruefen Eingabeparameter, NaN, Seam, CFL und Budget.

Build und Regression: `scripts/p2_nonlinear_mac_plic_build.cmd`
(MSVC C++17 Release, eigener Native-Test und die vorhandenen
106 CTest-Suites). Quantitative maschinenlesbare Abnahme:
`scripts/p2_nonlinear_mac_plic_acceptance.py` und
`ENGINE TEST STUBE/runs/p2_nonlinear_mac_plic_partial.json`.
Die Logs und SHA256-Quellfingerabdruecke sind im Report enthalten.

**Grenzen:** Vollstaendig periodische konstante-Dichte-Domaene;
Wasser ist eine passiv transportierte VOF-Marke, beeinflusst die
Impulsgleichung, den Druck, die Dichte und die Viskositaet **nicht**.
Das ist keine spannungskonsistente freie Wasseroberflaeche und kein
physikalisch geschlossenes Zwei-Phasen-Navier-Stokes-Modell.
Keine dynamische Fluid-Festkoerper-Gegenkraft, keine global
geschlossene mechanische Energie, keine gemessene 3D-CFD-Validierung,
kein GPU-/LOD- oder Unreal-5.8-Native-E2E fuer diesen Adapter.
P2 bis P8 bleiben INCOMPLETE; `phase_complete=false`.
