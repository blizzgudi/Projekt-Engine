# P2 Teilabnahme: zeitliche Kopplung viskoser nichtlinearer MAC/PLIC-Schritte

Datum: 2026-10-31. **PARTIAL, kein P2-Vollabschluss.**

## Umsetzung

Der neue optionale Header `include/upc/p2_viscous_mac_plic_sequence3d.hpp`
verkettet zeitlich unterschiedliche positive Zeitschritte und nichtnegative
kinematische Viskositaeten. Jeder Schritt fuehrt echte periodische explizite
MAC-Viskositaet, nichtlinearen Impulspredictor, Druckprojektion und
geometrisch konservativen passiven PLIC-Wassertransport aus. Der
vorherige Ausgabezustand wird als naechster Eingabezustand verwendet.
Die PLIC-Normalen werden im darunterliegenden Subcycling aktualisiert.

Der Adapter zaehlt **tatsaechliche** PLIC- und Viskositaets-Substeps
ueber die gesamte Sequenz. Ein ueberschrittenes Gesamtbudget oder ein
ungueltiges spaeteres Intervall wird abgewiesen. Das Eingabeobjekt
bleibt unveraendert; bei einem Fehler wird kein teilweise aktualisierter
Zustand an den Aufrufer zurueckgegeben. Pro Intervall werden
Volumen, Bilanzresiduum, Druck-Divergenz und viskose Face-Proxies
diagnostiziert. Nullviskositaet nutzt weiterhin den bestehenden
unveraenderten inviskosen Integrationspfad.

## Echte Referenzen und Grenzen

Unabhaengige diskrete Fourier-Referenz fuer periodische Scherung
`u(y)=0.02+0.04*sin(2*pi*j/n)`, `v=w=0`, auf 8^3 und 12^3.
Die nichtlineare Selbstadvektion verschwindet fuer dieses Feld.
Fuer jedes Intervall mit `nu>0` wird der analytische diskrete Faktor
`(1-4*alpha*sin(pi/n)^2)^substeps` multipliziert. Drei Intervalle
(`dt=[0.2,0.4,0.15] s`, `nu=[0.002,0.004,0] m^2/s`) werden
gegen alle U-Faces geprueft, inklusive periodischer Naht.

Getrennter 12^3-Test: drei unterschiedliche Intervalle mit einem
scharfen Wasser-Slab, wiederverwendeten MAC/VOF-Zustaenden,
Bitgleichheit mit drei manuell hintereinander ausgefuehrten
Einzelschritten, unveraenderter Eingabe, Volumen- und Divergenzpruefung.
Neun Negativtests pruefen leere/ungueltige Intervalle und
erschoepfte Gesamtbudgets.

Reproduktion unter `D:\Projekt Engine\Engine Core\UnifiedPhysicsCore`:
`scripts/p2_viscous_mac_plic_sequence_build.cmd` und
`python scripts/p2_viscous_mac_plic_sequence_acceptance.py`.
Der Build erzeugt einen eigenen MSVC-C++17-Nativetest, fuehrt ihn
tatsaechlich aus und prueft danach die bestehende vollstaendige
CTest-Regression. Die neue Datei ist **noch kein CTest-Target**.

## Erweiterung: Sequenzweites Wasserinventar (31.10.2026)

Der optionale Adapter erfasst jetzt initiales/finales Wasser-Volumen (m3),
Wassermasse bei vorgegebener konstanter Dichte (kg), Intervall-Grenzspruenge,
eine unabhaengig aufsummierte Teleskop-Bilanz, maximale Druckprojektions-
Divergenz und die Summe der drei viskosen Face-Geschwindigkeitssummen-Proxies.
Er verwirft eine unzulaessige Diskontinuitaet an einer Intervallgrenze
oder eine nicht schliessende Teleskop-Bilanz, statt einen scheinbar
konservativen Gesamtzustand auszugeben. Kein physikalischer Impuls- oder
Energiebeweis; die Face-Summen haben keine FSI-Impulseinheit.

Der native 8^3/12^3-Vollwasser-Eigenmodentest prueft die analytischen
Volumina, kg-Umrechnung, Bilanz und die Summe der einzelnen Reports.
Der 12^3-Wasser-Slab prueft unabhaengig 0.432 m3 bzw. 432 kg,
Null-Intervallspruenge und das Maximum der projizierten Divergenz.
Das Python-Gate wertet die native LEDGER-Zeile mit sieben weiteren
quantitativen Checks aus. Die Erweiterung veraendert die Advektion nicht.

Nachweis mit unabhaengigen Python-Formeln, Quell-/Binaer-/Log-SHA256:
`ENGINE TEST STUBE/runs/p2_viscous_mac_plic_sequence_partial.json`.

## Offene Vollabnahme

Die neue Sequenz ist **keine** spannungskonsistente Zweiphasen-CFD.
Konstante Dichte, periodische Grenzen, passive Wasserfraktion,
explizites Operator-Splitting. Keine dynamische Zweiwege-FSI, keine
Wasser/Boden-/Waerme-/Reaktionskopplung dieses Pfades, keine
physikalische globale Energie-/Impulserhaltung, keine externen
3D-CFD-Messdaten und kein neuer UE5.8-Native-E2E-Nachweis.
P2-P8 bleiben INCOMPLETE. Kein Push.
