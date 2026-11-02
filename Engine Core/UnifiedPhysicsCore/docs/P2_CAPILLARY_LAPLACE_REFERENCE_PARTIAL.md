# P2: unabhängiger 3D-Young-Laplace-Drucksprung als negativer Krümmungsnachweis (02.11.2026)

**Status: Teilmeilenstein, P2 NICHT vollständig abgenommen.**

## Physikalische Frage

Im bestehenden bewegten Zweiphasen-MAC/VOF-Kapillaroperator wächst die
numerische Restströmung eines ruhenden Tropfens bei Gitterverfeinerung.
Unklar war, ob der dichtegewichtete Druck-PCG selbst den Young-Laplace-
Drucksprung nicht ausgleichen kann oder ob die räumlich schwankende
diffuse VOF-Krümmung die Störung verursacht.

Der neue, bewusst getrennte Referenzoperator
include/upc/p2_capillary_laplace_reference3d.hpp nimmt für eine
hergestellte Kugel **die analytische Krümmung 2/R als externe Vorgabe**.
Er setzt auf allen inneren MAC-Flächen den exakten diskreten
Quellterm dt * sigma * (2/R) * delta(alpha)/(dx*rho_face) mit derselben
Dichte-Mittelung wie der produktive PCG. Anschließend läuft die **echte
native variable-Dichte-PCG-Druckprojektion**. Gegenreferenz ist
p_i = sigma * (2/R) * (alpha_i - mean(alpha)); zusätzlich werden die
reinen Wasser- und Luftzell-Druckmittel verglichen.

Die Routine mutiert den übergebenen Zustand nicht. Sie prüft die
physikalischen SI-Parameter, Phasendichten, Nullanfangsgeschwindigkeiten,
aufgelöste reine Phasen, PCG-Konvergenz und Kapillar-Zeitschrittgrenze.
Der vorhandene diffuse CSF-Kapillaroperator wird auf **demselben
VOF-/Dichtefeld** separat erneut ausgeführt, nicht durch die
analytische Krümmung ersetzt.

## Native positive und negative Ergebnisse

MSVC Release: drei echte 3D-Tropfengitter mit identischer physikalischer
Domäne 1,2 m Kantenlänge, sigma=0,072 N/m, R=0,31 m, dt=0,0005 s.

| Gitter | Analytischer Drucksprung | Max. PCG-Druckfehler | Restgeschwindigkeit mit exakter Krümmung | Restgeschwindigkeit mit diffuser Krümmung |
|---|---:|---:|---:|---:|
| 8³, dx=0,15 m | 0,4645161290 Pa | 6,66e-16 Pa | 1,96e-20 m/s | 5,9607e-7 m/s |
| 12³, dx=0,10 m | 0,4645161290 Pa | 2,25e-15 Pa | 1,92e-18 m/s | 1,0901e-6 m/s |
| 16³, dx=0,075 m | 0,4645161290 Pa | 3,61e-15 Pa | 2,03e-18 m/s | 3,0086e-6 m/s |

Der diskrete Druckprojektor kann den **vorgegebenen konstanten**
Young-Laplace-Drucksprung praktisch bis zur Rundungsgenauigkeit
ausgleichen. **Negatives, wichtiges Ergebnis:** Die diffuse
Krümmungsrekonstruktion erzeugt weiterhin spurious currents, die bei
den drei geprüften Gittern sogar ansteigen. Das ist **keine**
konvergente Oberflächenspannungsphysik. Die Gegenprobe zeigt eine
konkrete Fehlerquelle, beweist aber nicht, dass sie die einzige ist.

52 native Assertions PASS, darunter unabhängiger Drucksprung,
Divergenz, Impuls-/Energieprojektion, unveränderter Eingangszustand,
neun ungültige Eingaben und PCG-Unteriteration. Native Target-CTest
PASS; vollständiger CTest nach Registrierung **147/147 PASS**.
Die unabhängige Python-QA prüft die erneut ausgeführte Binärdatei,
alle drei Zahlenreihen, die negative Gitterkonvergenz, SHA256 und
manipulierte Protokolle. Alle Logs liegen unter
D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_laplace_reference_*.

## Grenzen / offene Vollabnahme

Die exakte Kugelkrümmung wurde **als Referenz eingespeist** und nicht
aus der tatsächlichen Wasseroberfläche rekonstruiert. Es wurde weder
ein PLIC-/Height-Function-Stressmodell noch ein balancierter
beweglicher Tropfen, dynamische FSI, globale kapillare Gesamtenergie,
MARIN/SPHERIC-H1-H4-/P1-P8-Messkonvergenz über >=6 s und drei Gitter
noch UE5.8 Native-E2E dieser neuen Kette nachgewiesen.

Der bestehende diffuse Kapillaroperator bleibt unverändert und
nicht konvergent. Die nächsten physikalischen Arbeiten müssen
eine lokal geometrisch konsistente Krümmung und denselben
diskreten Oberflächendrucksprung **ohne externe Krümmungsvorgabe**
realisieren und über statische und bewegte Tropfen unabhängig prüfen.
Keine Änderung am offiziellen P2-Voll-Gate.
