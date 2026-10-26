# Phase P5 – 3D-Festkörper, Bruch und Granulat: CPU-MVP-Teilnachweis

Stand: 2026-10-26. Projekt: D:\Projekt Engine.

**Validierter Teilumfang:** Reale MSVC-C++17-CPU-Prototypen für (1) deformierbare,
durch 3D-Volumen vernetzte Federgitter mit elastischem Bruch und
Material-Damage-Writeback, (2) stationäre Ebenenkontakte der
Festkörperknoten und (3) 3D-Korn-Korn- sowie Korn-Wand-Kontakte mit
Schwerkraft, Rückprall und begrenzter Coulomb-Reibung. Tests auf
Erhaltung, Belastung, Rissen, stabilen Kontakten, Referenzgesetzen,
Granulatstapeln und CPU-Rastergrößen.

**P5 bleibt PARTIAL.** Die nachgewiesenen CPU-Tests belegen Teilfunktionen,
nicht alle für die vollständige Phase nötigen belasteten Festkörper-,
Kontakt- und Granulatreferenzen. Die Resultate sind kein wissenschaftlich
kalibrierter FEM-/MPM-/DEM-Simulator und kein AAA-Release.

## Materialmodell und Engine-Integration

- MaterialLibrary-Material-ID muss Phase::Solid besitzen.
- Dichte aus zentraler Bibliothek bestimmt 3D-Masse:
  Lattice: M = N_nodes * rho * dx³ (vereinfachte nodale Lumped-Mass-
  Diskretisierung; Rand-/Oberflächengewichte noch nicht korrigiert).
  Korn: M = rho * (4/3) pi r³; Kugelform ist ausdrücklich geometrisch.
- Materialmechanische Zusatzparameter separat typisiert:
  Lattice stiffness N/m, break strain; DEM restitution [0,1] und
  dimensionsloser Coulomb-Reibkoeffizient >=0.
- Lattice-Zellkoordinaten können in VoxelGrid als dieselbe Material-ID
  exportiert werden. Einzelzell-Schaden = Anteil zerstörter
  angrenzender elastischer Bindungen (0..1). Keine fremden Material-IDs
  überschreiben. Massensumme bleibt zum Beispiel 4³ * rho * dx³.

## Biegung, Belastung und Bruch

- solid.hpp: 3D-Knotenmasse mit Geschwindigkeit, Newton-Kräften und
  Hooke-Federn; Bindung bricht, wenn |L-L0|/L0 die Materialgrenze
  überschreitet. Die im Moment der Trennung aus dem elastischen
  Modell entfernte Energie wird als fracture_released_j bilanziert.
  Das ist ein **Bruchenergiekonto**, kein thermisch gekoppelte
  Entropie-/Energie-Umwandlung (dafür wäre P4/P6 zusätzlicher Code nötig).
- solid_lattice3d.hpp: 26er-Nachbarschaft als einmalige räumliche
  Bindungen; verhindert reine Achsen-Laxheit bei Scherbelastung.
  Explizit stabilitätsbegrenzte Schritte, optionale fixierte Tragschicht.
  Intakte Bindungen bilden verbundene Fragmente; beschädigte
  zweischichtige Probe zerfällt in zwei 32-Knoten-Stücke.
- Keine FEM-Tetraeder, keine exakte Kontinuumselastizität,
  keine plastischen Dehnungs-/Spannungstensoren, keine reibungsfreie
  Selbstkollision von deformierten Spring-Faces, keine Kontaktkraft
  aus Triangulationsflächen. Scher-/Volumenantwort nur über
  Gitterfedern angenähert.

## Kontakt / Granulat

- solid_contact3d.hpp: Bereits P5-teilvalidierter Kontakt
  mit einer unbeweglichen Ebene, beliebiger Normale, diskretem
  Kollisionsimpuls, Restitution, Coulomb-Impulsreibung und
  statischer Ruheschwelle. Test im 10.000-Schritte-Ruhefall.
- granular3d.hpp: Kugelkörner in einer 3D-Box mit sechs
  Wandebenen, paarweisen Kornkontakten, normalen und tangentialen
  Stoßimpulsen. Materialkombination: min(e_a,e_b) und
  sqrt(mu_a*mu_b). Geometrische Überlappung wird iterativ
  entlang der Kontakt-Normalen verteilt (Masseninversion).
  Adaptive einfache Zeitschritt-Unterteilung auf Basis
  begrenzter Kornverschiebung; 12 Kontaktiterationen je Substep.
  Wand-Nichtdurchdringung am Schrittende, belastete
  27-Korn-Pile unter g=9,81 m/s² getestet.
- Elastischer Stoß zweier gleich schwerer Körner:
  relative kinetische Energie nach Kontakt = e² * vorher,
  e in {0, 0.5, 1}; Impuls der beiden Körper erhalten.
- Kornfriktion ändert den Relativtangentialimpuls; bei
  Null-Gravitation dürfen diese Stoßmodelle Energie nicht erzeugen.

## Quantitativer Teilnachweis

Native Suite:
  tests/solid_tests.cpp
  tests/solid_contact3d_tests.cpp
  tests/solid_lattice3d_tests.cpp
  tests/granular3d_tests.cpp
  benchmarks/p5_reference.cpp

Die Messung/Validierung wird über scripts/p5_acceptance.py
und alle bisherigen CTest-Gruppen vorgenommen:
1. MSVC-Release-Build + CTest alle PASS.
2. Hookesche Federenergie U=0.5*k*(DeltaL)² auf unabhängiger
   2-Knoten-Probe: k=1000 N/m, 0,01 m Dehnung,
   erwartete Energie 0,05 Joule.
3. Elastischer, halbelastischer, vollständig inelastischer
   gleicher 3D-Zweikörperstoß; kinetisches Energieverhältnis
   genau e²; Paar-Gesamtimpuls erhalten; keine Überlappung.
4. Materielle 3D-Lattice-Bruchprobe 4³ mit 64 Punkten,
   nach Belastung exakt 2 Fragmente mit je 32 Knoten.
   Bruchenergie positiv, ursprüngliche Materialmasse erhalten.
5. Export von Material-ID, Damage und konservierter 3D-Masse
   auf gemeinsames VoxelGrid.
6. Eine 27-Korn-Schwerkraft-Pile hält Boden- und
   Pair-Überlappungsgrenzen ein, auch unter wiederholten
   Integrationsschritten. Massenfehler unter 1e-11 kg.
7. Granulat-CPU-Messungen bei 8/27/64 Körnern,
   Lattice-CPU-Messungen bei 64/216/512 Knoten.
   Es werden gemessene ms und Bondzahlen dokumentiert;
   keine garantierten AAA-Frameraten.
8. Native Input-Härtung und Alter-Code-Regressionen P0–P4.
9. Maschinenlesbarer **Teilbeleg** mit mindestens 11 Referenzfällen,
   festgelegten numerischen Gates und SHA256-Quellfingerprints.

Skripte/Nachweise:
  scripts/p5_contact_partial.py  (historischer Teilnachweis)
  scripts/p5_acceptance.py
  ENGINE TEST STUBE/runs/p5_acceptance.json
  scripts/phase_gate.py

## Ehrlich abgegrenzte, weiter offene Physik

- DEM verwendet O(N²) Kornpaarprüfungen und keinen Broadphase-
  Raumindex; bei vielen Körnern nicht game-echtzeitfähig. Kein
  Korn-Torque/Rotation, keine rolling friction, kein Cohesion/
  Plastizität, keine Sand-Mehrphasen-Kontinuumskopplung.
- Kollisionen sind diskret. Hohe Geschwindigkeiten,
  sehr kleine Körner und viele Körper verlangen adaptive
  Zeitschritte; es gibt keinen kontinuierlichen Swept-TOI-
  Kontakt und keinen allgemeinen Stapel-/Constraint-Solver.
- Spring-Lattice ist kein kalibriertes FEM- oder MPM-Modell.
  Kontinuierliche Mesh-/FEM-Oberflächenkontakte sind offen;
  sphärische nichtgebundene Knotenkontakte sind als CPU-MVP implementiert.
- Bruchkontakt und Wasser/Boden mit Impulsaustausch sowie
  gesamte thermo-mechanische Energie werden nicht gleichzeitig
  erhalten. Dem P5-MVP fehlt daher die globale, vollständig
  bidirektionale 3D-FSI und Gesamtenergierechnung.
- Kein Solver-Nachweis für Sandrutschen an frei geformten
  Meshoberflächen, keine skalierte GPU-Granularphysik und
  keine Unreal-PIE-Visualisierung. P7/P8 bleiben offen.

## Erweiterte P5-3D-Selbstkontakt-/Konstitutiv-Abnahme

Die anfängliche P5-Teilabnahme (plane/granular/lattice) alleine genügte
nicht für eine P5-Abschlusserklärung. Ergänzt:

- **DeformableContact3D:** Ein oder mehrere dynamische Federgitterkörper
  mit sphärischen volumetrischen Knotenkontaktproxies.
  Nicht benachbarte Knoten dürfen sich selbst berühren; feste noch
  intakte Federbindungen sind ausgenommen. Kontakt derselben oder
  verschiedener deformierbarer Körper wird über inverse Massen,
  Projektion, Stoßimpulse und Coulomb-Reibung behandelt.
- **Gekoppelte Bruch/Kontaktprüfung:** Zwei ursprünglich durch eine
  Feder verbundene Knoten trennen sich bei Überschreiten der
  Dehnungsgrenze. Danach kollidieren sie wie getrennte Fragmente.
  Die isolierte kinetische Impulsbilanz stimmt mit dem errechneten
  e²-Verhältnis überein; dissipierte Impulsenergie wird getrennt
  erfasst, ebenso die positive Bruch-Federenergiefreisetzung.
- **Affine 3D-Lattice-Materialantwort:** Für 4³/6³/8³ Gitter
  werden axiale ε=0,001/0,002/0,004 sowie Scherspannungs-
  Antwort und elastische Energie quantitativ vermessen.
  Axiale Kraft bei verdoppelter Dehnung ungefähr verdoppelt;
  Energie verhält sich näherungsweise quadratisch, Quer-
  Restkraft unter rein axialer Dehnung bleibt numerisch klein.
  Dies ist ein *diskreter konstitutiver Konsistenztest*, keine
  Kalibrierung des Young-Moduls gegen Laborversuche.
- **Quantitative Full-CPU-MVP-Gate:** scripts/p5_full_acceptance.py
  prüft alle ursprünglichen Referenzen plus Selbstkontakt, dynamische
  Bruchkontakt-Folge und numerisches Last-/Steifigkeitsverhalten.
  Maschinenbericht: ENGINE TEST STUBE/runs/p5_full_acceptance.json,
  zusätzlich Quell-SHA256. Diese Gate muss P5 im Gesamt-
  phase_gate.py vor einem PASS ausdrücklich bestätigen.

Die Grenzen sind weiterhin ernst zu nehmen: sphärische Knoten
bilden kein facettengetreues deformierendes Oberflächennetz.
Beliebige Stapel, Rotationsmomente, Sand-Druckgesetz/MPM,
hochbelastete Kontakte, kontinuierliches Kollisions-Timing
und gekoppelte Impuls-/Wärmebilanz aller Materialien sind nicht
wissenschaftlich validiert. Der vollständige P5-CPU-MVP ist
deshalb nur die **reale, abgegrenzte** Material-/Bruch-/
Kontakt-Demonstration, nicht die fertige AAA-Festkörperphysik.
