# P2-Teilnachweis: 3D-SDF mit viskosem Normalspannungssprung (01.11.2026)

## Abgrenzung

Neuer optionaler Baustein `include/upc/p2_sdf_normal_stress3d.hpp`: Aus einer **extern vorgegebenen** 3D-Signed-Distance-Funktion, Young-Laplace-Kruemmung und den **vorgegebenen** 3x3-Geschwindigkeitsgradienten beider Phasen wird die normale Spannungsrandbedingung am echten fraktionalen MAC-PCG-Face berechnet:

`p_w - p_a = sigma * kappa + 2 * (mu_w * n.D_w.n - mu_a * n.D_a.n)`.

`D=(grad(u)+grad(u)^T)/2`; `n` zeigt von Wasser zu Luft. Die 3D-Normale wird aus dem SDF-Gradienten auf die Schnittstelle interpoliert, normalisiert und fuer den viskosen Spannungssprung verwendet. Druck und geometrischer Schnittabstand werden gemeinsam in die existierende native 3D-MAC-PCG-Projektion uebergeben. Negative/nichtfinite Viskositaet, nichtfinite Gradienten, Maskenkonflikte und Geometriefehler werden vor der Mutation abgewiesen. Bei `mu_w=mu_a=0` ist die alte Young-Laplace-Bedingung exakt wiederhergestellt.

**Kein Vollnachweis:** Die Gradienten beider Phasen sind vorgegeben, **nicht** aus einem gekoppelten dynamischen Luft-/Wasser-Stroemungssolver gewonnen. Die tangentiale Spannungskontinuitaet und die tangentiale Marangoni-Kraft fehlen. Die VOF/SDF-Kruemmung ist noch nicht aus dem laufenden 6-s-MARIN-VOF-Feld hergeleitet. Die 3D-Normalspannung allein ist **keine vollstaendig stresskonsistente freie Oberflaeche**.

## Echter Native-Nachweis

Reproduktionsbefehl: `scripts/p2_sdf_normal_stress_build.cmd`. Windows MSVC Release, 129/129 CTest PASS und **5.871 native Assertions PASS**. Gemessene Ergebnisse:

- Planare x-Schnittstelle: 144 Faces; `mu_w=2 Pa*s`, `mu_a=0.5 Pa*s`, `du_w/dx=0.3/s`, `du_a/dx=-0.1/s`; analytischer normaler viskoser Drucksprung **1,3 Pa**. PCG-Divergenz **8,52896e-13 1/s**, maximale x-Face-Geschwindigkeit **3,62803e-14 m/s**, maximaler Druckfehler **1,76044e-10 Pa**.
- Kugel (R=0,287 m): bei `dx=0,08 m` 264 Faces, maximale Abweichung des 3D-Normalspannungssprungs gegen unabhaengig radial berechnete Normale **0,0311568 Pa**; bei `dx=0,04 m` 984 Faces und **0,0104853 Pa**. Verbesserungsfaktor **2,97**. Antisymmetrische Rotationsgradienten aendern `n.D.n` nur innerhalb der Gleitkommaabweichung (max. **1,42109e-14 Pa**).
- Fuenf absichtlich ungueltige Eingaben werden abgelehnt; 144 Nullviskositaets-Faces reduzieren exakt auf den alten SDF-Kapillardruck. Luftdruck-Offset +100 Pa aendert die Grenzdruckwerte um genau +100 Pa.

Unabhaengige QA: `scripts/p2_sdf_normal_stress_acceptance.py` validiert die numerischen Grenzen, fuehrt das native Binary erneut aus, prueft absichtlich verfaelschte Traces und schreibt Source-/Binary-/Log-SHA256 nach `ENGINE TEST STUBE/runs/p2_sdf_normal_stress_partial.json`. Rohlogs: `p2_sdf_normal_stress_{build,native,ctest}.txt`. Offizielles `phase_gate.py` bleibt verbindlich.

## Negative Messlage und offene Vollabnahme

Der vorhandene reale MARIN/SPHERIC-6-s-H1-H4-Abgleich bleibt **FAIL**. Dieser Lauf erzeugt bewusst **keine** neue kostspielige MARIN-Simulation und **keine** behauptete P1-P8-Drucksensor- oder Drei-Gitter-Messkonvergenz. Weiter fehlen dynamische Zweiwege-FSI mit physikalisch geschlossenen Impuls-/Energie-/Massenbilanzen, vollstaendige Luft-/Wasser-Normal- und Tangentialspannungen sowie ein Unreal-5.8-Native-E2E fuer diesen neuen Pfad. **P2 bleibt PARTIAL / phase_complete=false.**
