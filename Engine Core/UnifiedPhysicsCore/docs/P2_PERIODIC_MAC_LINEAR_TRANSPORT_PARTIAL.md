# P2 — konservativer periodischer MAC-Lineartransport (Teilnachweis)

Stand: 28.10.2026. **P2 PARTIAL, keine Vollphasenabnahme.**

## Implementierung

`include/upc/p2_periodic_mac_transport3d.hpp` enthält den eigenständigen
`P2PeriodicMacTransport3D::step` für ein periodisches 3D-MAC-Gitter.
Alle drei gestaffelten Geschwindigkeitskomponenten werden als getrennte
Skalarfelder mit einem expliziten konservativen 3D-Upwind-Operator erster
Ordnung transportiert. Die Transportgeschwindigkeit ist räumlich konstant
und **extern vorgeschrieben**, nicht die lokale Fluidgeschwindigkeit.

Für jede Komponente und jede eindeutige periodische Gitterstelle gilt

`q_new = (1 - Cx - Cy - Cz)*q + Cx*q_upwind_x + Cy*q_upwind_y + Cz*q_upwind_z`

mit `Cd=abs(v_transport_d)*dt/dx` und `Cx+Cy+Cz <= 1`.
Die redundant gespeicherten oberen U/V/W-MAC-Grenzflächen werden exakt
von der jeweiligen unteren periodischen Ebene übernommen. Das vermeidet
eine fälschliche Doppelzählung der konservierten Komponentenintegrale.
Unpassende Dimensionen, nichtperiodische Seams, NaN/Inf und CFL-Verletzungen
werden vor einer Zustandsänderung abgelehnt. Die Eingabevektoren sind
`const`, das Ergebnis ist ein neuer Datensatz.

## Unabhängige diskrete Referenz

Auf **geraden 3D-Gittern** ist das alternierende
`(-1)^(i+j+k)` ein Eigenvektor des periodischen Upwind-Operators.
Bei `(Cx,Cy,Cz)=(0.07,0.11,0.13)` ist der exakte
Amplitudenfaktor pro Schritt `G=1-2*(0.07+0.11+0.13)=0.38`.
Daraus folgen für fünf Schritte:

- Komponentenmittelwerte und Summen bleiben erhalten;
- `q_k = mean + amplitude * G^k * (-1)^(i+j+k)`;
- die zentrierte diskrete Quadratsumme skaliert mit `G^(2*k)`.

Die native Prüfung deckt `8x10x12`, `12x14x16` und
`16x18x20`, acht Vorzeichenkombinationen, fünf Schritte,
alle drei MAC-Komponenten, redundante periodische Grenzebenen,
Konstantfelder, Nulltransport und Negativfälle ab.
Das Python-Gate prüft die unabhängig hergeleitete Eigenmode und
Dissipation aus der nativen Ausgabe, den vollständigen CTest-Lauf und
die SHA-256-Fingerprints. Die konkreten Messergebnisse stehen in
`ENGINE TEST STUBE/runs/p2_periodic_mac_transport_partial.json`
und `ENGINE TEST STUBE/runs/p2_periodic_mac_full_ctest_20261028.txt`.

## Grenzen und nächste Arbeit

Dieser Operator ist ein **linearer Transport-Baustein**, kein
physikalisch vollständiger Impulsadvektionsschritt. Er verwendet
periodische Grenzen und einen vorgegebenen konstanten Transportvektor;
keine freien Oberflächen, keine bewegten Cut-Cell-Wände, keinen
nichtlinearen `u·grad(u)`-Term, keine Druckprojektion oder
bidirektionale Festkörper-Fluid-Kräfte. Die zentrierte Quadratsumme
ist lediglich eine numerische Energiediagnose, keine globale
kinetische Energie-/FSI-Bilanz.

Nächster P2-Schritt: konservative staggered-Dual-Cell-Advektion mit
lokalen, diskret divergenzfreien MAC-Transportflüssen und
Cut-Cell-Randbehandlung, anschließend Integration in
`P2PrescribedBoxMacVof3D` und `CoupledMacWater3D`.
Unabhängige gemessene 3D-CFD-Referenzen und neue
Unreal-5.8-Native-E2E-Nachweise bleiben erforderlich.
