# P3 – Material-/Volumen-Kopplung: 3D CPU-MVP Abnahme
Stand: 2026-10-26. Projekt: D:\Projekt Engine.

## Abgegrenzter Meilenstein

P3 validiert die INTERAKTION zwischen frei strömendem 3D-MAC-Wasser,
zentral registrierten Voxelmaterialien (durchlässiger Boden/undurchlässiger
Fels), lokalem Wasser in Poren und einem beweglichen rigiden Voxel.
Dies ist eine **vereinfachte, vollständig testbare CPU-Referenzimplementation**.
KEINE vollständige Stokes-/Richards-Darcy-Simulation und keine
kontinuierliche zweiphasige oder hochauflösende Fluid-Structure-Interaction.

## Architektur und echte Kommunikation

- include/upc/material_coupling3d.hpp: MaterialVolumeCoupler3D verbindet
  UnifiedPhysicsRuntime, CoupledMacWater3D und das unverändert für andere
  Phasen verfügbare VoxelGrid. Material-IDs müssen in MaterialLibrary
  vorhanden sein und die passende Phase besitzen.
- register_porous_material(material_id, porosity, conductivity_m_s):
  Einmalige Materialparameter pro Material-ID; pro Voxel optional abweichende
  Parameter. Pore water wird als lokaler laufender Zustand geführt.
- set_soil_from_material und set_soil synchronisieren das Material-Voxel
  als Festkörpermaske mit dem MAC-Solver (Wasser darf in den Poren gespeichert
  werden, aber nicht im massiven Volumen des Bodenvoxels).
- step(dt) führt MAC-Wasserbewegung, danach vertikale Wasser-Boden-
  Infiltration und Wassertransfer im Bodengitter aus.
- Durchlässige Bodenlagen, maximale Porenkapazität, trocken/satt,
  undurchlässige Felsen werden unterschieden.
- wall_force berechnet die Druckkraft einer 3D-Wasserverteilung auf
  fest stehende starre Voxel aus dem MAC-Druck.
- set_rigid_body und step_body führen eine dynamische, ausschließlich
  vertikal bewegbare Ein-Zellen-Körpermasse mit Druck/Schwerkraft,
  Geschwindigkeitsänderung und konservativer Wasserverdrängung aus.
  Nach Bewegung wird der MAC-Fluid-Step unter neuer Kollisionsgeometrie
  weiter getestet.

## Conservation/Physik des Porenflusses

Ein einzelner vertikaler Interfacefluss folgt unter druckfreiem Eintritt
dem einfachsten Darcy-artigen Infiltrationsversuch:

  Q = K * A * Δt

wobei K in m/s, A=dx² und Δt in s. Die hydraulische Wasserdruckhöhe im
freien Wasser vergrößert den Fluss; die Speicher-/Quellgrenzen begrenzen
ihn. In der oberen Bodenlage fließt Wasser weiter nach unten, sofern der
Boden verbunden und noch freie Porenkapazität vorhanden ist. Keine seitliche
Sickerströmung, keine Saugspannung, kapillarer Aufstieg oder
unsaturierte Richards-Gleichung.

Jeder positive Transfer m_3 von Voxelzentrum z+dx nach z:
- zieht exakt dieses Wasservolumen von der Quelle ab,
- addiert genau dasselbe Volumen in der Bodenpore,
- reduziert seine potentielle Energie um ΔE = rho*g*m_3*dx,
- schreibt denselben Betrag in ein ausgewiesenes, kumuliertes
  Darcy-Dissipationskonto.

Daher ist die Bilanz über den isolierten Poren-Austausch in jedem Test:
  Wasser_masse_nachher - vorher ≈ 0
  (E_grav_nachher + E_diss_nachher) - (E_grav_vorher + E_diss_vorher) ≈ 0

Das Bilanzkonto darf nicht mit einer global konservierten Energie des
gesamten dreidimensionalen Fluid-Solvers verwechselt werden: der
semi-Lagrangesche MAC-Transport verursacht numerische Dissipation, die
nicht in das Wärmefeld von P4 übertragen wird.

## Kräfte und grobe Rigid-Body-Bewegung

Normaldruck auf ein rigides Voxel: F = p_face*A, Vorzeichen durch
äußere Flächennormale. Der freie MAC-Druck wird auf die Fläche
hydrostatisch über einen halben Zellabstand rekonstruiert.
Für ein 0,1-m-Würfelvolumen in ruhendem Wasser (rho=1000 kg/m³),
erwartete Auftriebskraft = rho*g*dx³ = 9,81 N. Gemessener
Druckkraftanteil beim repräsentativen statischen Test: 9,79695 N
(relativer Fehler ca. 0,13 %).

Rigid body mechanical work:
  Δ(E_kin + E_grav) = F_fluid * Δz
für ein Substep mit mittlerer Trapez-Position und konstant behandeltem
Druck über den Zeitschritt. Kollisionszelle wird erst bei Erreichen der
neuen Voxelposition gewechselt; vorhandene Zielzellenflüssigkeit wird
in das freigewordene Quellvoxel umgebucht. Dadurch ist die reine
Wasservolumensumme beim Körperumzug konserviert.

**Begrenzung:** Die Druckkraft-Rückwirkung wird noch nicht als
physikalisch vollständig konsistente bewegte MAC-Randbedingung mit
konservativer Wasser-Impuls-/Energieübertragung gerechnet. Es handelt sich
um einen **quantisierten Zweiwege-Demonstrator für Geometrie und Volumen**,
NICHT um voll validierte hochpräzise FSI. Kontinuierliche Festkörpermechanik,
Bruch/Kontakt und granularer Boden gehören zu P5; global gekoppelte
Impuls-, Wärme- und Energiehaushalte erfordern weitere Arbeit (P4/P5).

## Native Abnahme / quantitative Referenzen

1. Materielle IDs/Phasen und Materialparameter, Überschneidungen,
   trocken/gesättigt/undurchlässig validieren.
2. Darcy-Einzelfläche K=0, 0,002 und 0,004 m/s; dx=0,1, 0,05,
   0,025 m. Genau Q=K dx² dt, dt=0,05 s, neun Referenzreihen.
3. Einfallender Regen auf 3D-Boden-Schichten, vertikale Porendrainage,
   Massenerhaltung und positive Speicherbegrenzung auf dynamischem MAC.
4. Infiltrations-Energie: frei/Poren-Potential und dissipierte
   Gravitationsarbeit; isolierte Bilanz < 1e-7 J.
5. Druckkraft/Würfel: positive 3D-Kraft, Abweichung von
   analytischem Auftrieb < 5 %.
6. Bewegter Body aktualisiert Material-ID, Kollisionsgitter und
   MAC-Fluid-Zustand; hält Gesamtwasservolumen innerhalb 1e-8 m³.
7. Körperbezogene (nicht globale Fluid-)Arbeitsbilanz < 1e-8 J.
8. Skaliert auf 8³, 12³ und 16³, 20 Fluid-Simulationsschritte
   (0,2 s Simulationszeit), Zeitmessung in ms und konservierte Masse.
9. Ganzer CMake/MSVC-Release-Build + alle Regressionen (P0–P2)
   PASS, maschinenlesbarer Acceptance-Report inkl. Source-SHA256.

Tests: tests/material_coupling3d_tests.cpp
Benchmarks: benchmarks/p3_reference.cpp
Gate: scripts/p3_acceptance.py
Evidence: ENGINE TEST STUBE/runs/p3_acceptance.json
Gesamtstatus: scripts/phase_gate.py

## Forschungsabgrenzung / Quellen

Die Verwendung eines einzelnen Interfaceflusses und Massenerhaltung
entspricht dem Grundgedanken lokaler konservativer Stokes-Darcy-Kopplung,
aber nicht deren voller mathematischer Lösung:

- Boon (2020), A parameter-robust iterative method for Stokes–Darcy
  problems retaining local mass conservation:
  https://numdam.org/articles/10.1051/m2an/2020035/
- COMSOL Theory – Free and Porous Media Flow, Darcy coupling:
  https://doc.comsol.com/6.3/doc/com.comsol.help.cfd/cfd_ug_fluidflow_porous.10.102.html
- Celia & Binning (1992), Mass-conservative flow in unsaturated media:
  https://doi.org/10.1029/92WR01488

Alle Abnahmemessungen sind direkt lokal aus unserem eigenen
C++-Prototyp reproduzierbar, keine Leistungsversprechen oder
Fremdimplementierungen.
