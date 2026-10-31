# P2 Teilnachweis: periodische viskose MAC-Impulsfortschreibung mit geometrischem PLIC

Datum: 2026-10-31. Status: **Teilmeilenstein**, P2 bleibt **INCOMPLETE**.

## Umfang

Neuer, rein optionaler Header `include/upc/p2_viscous_nonlinear_mac_plic3d.hpp`.
Die Funktion `p2_viscous_nonlinear_mac_plic_step3d` berechnet auf einem
periodischen 3D-MAC-Gitter zuerst explizite kinematische Viskositaet auf
allen drei gestaffelten Geschwindigkeitsfeldern. Danach folgt der bereits
bestehende nichtlineare Impuls-Predictor mit Druckprojektion und anschliessend
der geometrische, konservative PLIC-Transport eines passiven Wasseranteils.

Die periodischen duplizierten Face-Naehte werden nach jedem Diffusionsschritt
erneuert. Der explizite 7-Punkt-Laplace-Operator wird mit
`alpha = nu * (dt / substeps) / dx^2 <= 1/8` betrieben. Ueberschrittenes
Substep-Budget, nichtfinite Werte, falsche Feldformen und nichtperiodische
Face-Naehte werden abgewiesen. `nu=0` delegiert unveraendert an den
vorhandenen Integrationsschritt. Eingangsdaten werden nicht mutiert.

## Unabhaengige numerische Referenzen

Periodische reine Scherstroemung `u(y)=0.02+0.04*sin(2*pi*j/n)`,
`v=w=0`. Ihre nichtlineare Selbstadvektion ist exakt null.
Die diskrete 3D-Laplace-Eigenmode hat Eigenwert
`lambda=4*sin(pi/n)^2` und Amplitudenfaktor
`(1-alpha*lambda)^substeps`. Referenzen mit 8^3 und 12^3,
einem bzw. zwei Viskositaets-Substeps, vergleichen alle U-Faces
einschliesslich Naehte gegen diese **analytische diskrete** Loesung.
Die einmalige Summe der eindeutigen MAC-Face-Quadrate muss abnehmen,
die Summe jeder Geschwindigkeitskomponente erhalten bleiben.

Ein getrennter 12^3-Test prueft, dass die viskos fortgeschriebene
Geschwindigkeit einen scharfen 3D-Wasserblock tatsaechlich transportiert,
die Wasserbilanz schliesst und die Druckprojektion divergenzfrei bleibt.
Ein Nullviskositaetsfall vergleicht alle Wasser- und Face-Werte bitgenau
mit dem alten Operator. Sechs ungueltige Eingaben muessen scheitern.

Reproduzieren:
`scripts/p2_viscous_nonlinear_mac_plic_build.cmd`
und anschliessend
`python scripts/p2_viscous_nonlinear_mac_plic_acceptance.py`.

Nachweise:
`ENGINE TEST STUBE/runs/p2_viscous_nonlinear_mac_plic_partial.json`,
Native-/Compiler-/CTest-Logs und SHA256-Quell- und Binaerfingerabdruecke.

## Grenzen und noch offene Vollabnahme

Dies ist ein Operator-Splitting-Modell mit **konstanter Dichte** und
**periodischen Grenzen**. Der Wasseranteil ist passiv; Viskositaet,
Druck und Grenzflaechenbedingungen sind nicht zweiphasig konsistent.
Die Face-Quadratsumme ist ein numerischer Dissipationsindikator,
**keine physikalische globale Energie- oder Wandimpulsbilanz**.
Es fehlen insbesondere dynamische Fluid-Festkoerper-Gegenkraefte,
spannungsrichtige freie Oberflaechen, gemessene 3D-CFD-Referenzen
und ein neuer UE5.8-Native-E2E-Lauf dieses Adapters. P2-P8 bleiben
unabgenommen. Es erfolgt kein Unreal- oder GitHub-Push.
