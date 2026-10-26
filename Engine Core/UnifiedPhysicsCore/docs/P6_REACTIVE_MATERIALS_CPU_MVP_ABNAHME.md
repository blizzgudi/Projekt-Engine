# P6 — Dynamische 3D-Materialmischungen und Reaktionsregeln: CPU-MVP-Abnahme

Stand: 2026-10-26. Projekt: \`D:\Projekt Engine\`.
**Abnahmeumfang:** lokal ausgeführte, synthetische elementbalancierte
Stoffumwandlung mit temperaturabhängiger Reaktionskinetik,
3D-Massen- und Wärmediffusion, lokaler Zusammensetzung je Voxel,
nachverfolgten chemischen/sensiblen Energiekonten sowie überprüfter
Wärmekopplung an den P4-Thermalkern.

P6-PASS bedeutet **nicht**, dass beliebige reale chemische Reaktionen
automatisch bekannt, prognostizierbar oder experimentell kalibriert sind.
Eine vollständige 3D-reaktive Mehrphasen-CFD liegt NICHT vor.

## Architektur

1. \`include/upc/reactive_mixture3d.hpp\`:
   \`ReactiveMixtureVolume3D\` ist ein eigenes, vollwertiges 3D-CPU-
   Materialfeld mit aktiver MaterialLibrary-Anbindung.
   Jede Zelle führt ein *beliebig zusammengesetztes mol-Inventar pro
   registrierter Material-ID*, sensible Enthalpie in Joule und den daraus
   berechneten thermischen Zustand in Kelvin. Dies ist die kanonische
   Zusammensetzung; VoxelGrid enthält nur eine für den Material-Proxy
   exportierte dominante ID, Füllmenge und Temperatur. Die chemische
   Zusammensetzung geht beim Proxy-Writeback **nicht** verloren,
   darf aber aus diesem verlustbehafteten Bild nicht rekonstruiert werden.
2. Jedes synthetische Element hat eine Molmasse (kg/mol).
   Jede Spezies hat eine feste Formelsummen-Komposition als
   map(Element-ID -> Anzahl), Material-ID, Molmasse, Wärmekapazität,
   Diffusivität und spezifizierte Bildungsenthalpie. Molmasse muss
   der Summenformel entsprechen; Dichte stammt aus MaterialLibrary.
3. ReactionRule3D: Reaktanten haben negative stöchiometrische
   Koeffizienten, Produkte positive. Die Registrierung scheitert,
   wenn Elementbilanz oder Massenerhaltung verletzt werden.
   Die geschlossene kinetische Modellgleichung lautet:
   \`rate(T) = rate_1_s * exp(-Ea/(R*T))\`,
   \`extent = limiting_mol * (1 - exp(-rate(T)*dt))\`.
   Der Übergang erfolgt nur innerhalb vorhandener Reaktanten und
   stößt keine Produkte aus dem Nichts ab. Er ist eine **bewusst
   idealisierte Pseudo-1.-Ordnung-Regel**, nicht ein allgemeines
   chemisches Massenwirkungsgesetz zweiter Ordnung.
4. Reaktionswärme ist Bilanz, keine erfundene Wärmequelle:
   \`E_chemical = sum(species_mol * formation_enthalpy_j_mol)\`.
   Reaktionen verändern das chemische Reservoir um \`delta E\`;
   die lokale sensible Energie wird genau um \`-delta E\` erhöht.
   \`E_total = E_chemical + E_sensible\` bleibt ohne externe
   Wärmezufuhr konstant. Positive und negative Kelvin-Grenzen
   sowie überfüllte Zellvolumina werden validiert.
5. Dreidimensionale Fick'sche Diffusion mit 6er Nachbarschaft:
   \`delta_n = D * dt/dx² * (n_source - n_dest)\`,
   mit begrenzter Quellmenge und Zielvolumen. Feststoffspezies
   werden nicht als selbst diffundierend behandelt. Pro Paar
   wird sensible Energie mit den übertragenen Stoffpaketen
   umgebucht, chemische Energie folgt dem mol-Inventar.
   Gesamte Atome/Masse/chemische+sensible Energie werden geprüft.
   Der CPU-Versuch ist sequentiell; die Flussverteilung ist
   daher richtungs-/Schrittweiten-abhängig, nicht ein voll
   konvergierter advektiver CFD-Solver.
6. P4-Wärmeaustausch \`reactive_thermal_bridge.hpp\`:
   Ein Chemievoxel und ein *benachbarter*, physikalisch getrennter
   P4-Festkörper-Wärmekörper austauschen symmetrisch Joule nach
   einer durch Kapazitäten gedeckelten Fourier-Kontaktregel.
   Summe aus Chemie-/P4-Energie wird erhalten, auch nach
   exothermer Reaktion. Keine Doppelzählung ein- und derselben
   physikalischen Masse in zwei Wärmeregistern zulässig.
   Die Kopplung ist expliziter thermischer Kontakt, **kein**
   voll gekoppeltes Mehrmaterial-Reaktions-Fluidsystem.

## Reproduzierbare numerische Referenzen

- \`tests/reactive_mixture3d_tests.cpp\`: analytische limitierte
  X+Y->XY-Kinetik, Element-Massenerhaltung, Reaktionswärme,
  volle Stoffmenge, Ausgabe dominanter Material-ID ins VoxelGrid,
  3D-Fick-Transporte (200 Schritte), Invalid-Input und
  Volumengrenztests. Synthetische X/Y-Molmassen 0,010/0,020 kg/mol;
  XYZ-Produkt XY = 0,030 kg/mol. Synthetisches delta-H=-12.000 J/mol.
- \`tests/reactive_thermal_bridge_tests.cpp\`:
  Exothermer Reaktionszustand überträgt Wärme an getrennten
  P4-Festkörper. Beide Energie-Reservoirs + Atommassen konstant,
  100 aufeinanderfolgende thermische Kontaktschritte.
- \`benchmarks/p6_reference.cpp\`:
  Arrhenius-Referenz bei 285/320/380 K für E_a=14 kJ/mol,
  vorgegebene Modellparameter; 3D-Fick-Referenzen auf 8³, 12³,
  16³ Gittern mit 40 Schritten, gemessene CPU-Millisekunden.
  Das ist eine analytische *Modellgleichungs*-Abnahme, kein
  unabhängiger Laborbefund über tatsächliche Reaktionskinetik.
- \`scripts/p6_acceptance.py\`: erhebt messbare Referenzwerte,
  testet Grenzen/Monotonie, Regressionen, prüft SHA256
  der relevanten Quelldateien. Maschinenlesbarer Bericht unter
  \`D:\Projekt Engine\ENGINE TEST STUBE\runs\p6_acceptance.json\`.
- \`scripts/phase_gate.py\`: P6-PASS ist nur zulässig, wenn
  CTest plus sämtliche quantitative P6-Gates erfolgreich sind
  und die Source-Fingerprints zur realen Version passen.

## Ausdrückliche physikalische Grenzen

- **Keine** Elementperiodensystem-Datenbank, automatische
  Stoffmodellierung, Reaction Discovery oder chemisch kalibrierte
  Mischungs-Dichte/Volumenkontraktion, Ionen-/pH-/Elektronen-Physik,
  Radiolyse oder Spektren.
- Fick-Prototyp verarbeitet aktuell gelöste Spezies mit
  konstanter registrierter D; keine mehrphasige poröse
  Adsorption/Desorption oder Turbulenz-/MAC-Advektionskopplung.
- P3/Darcy, P4/Wärme und P5/Festkörper bilden noch
  **keinen** global konservativen reaktiven FSI-Thermosolver.
  Material-IDs und 3D-Raster sind kompatibel; beliebige
  zwei-Wege-Rückwirkungen auf Elastizität, Phasenübergänge,
  3D-MAC-Gasdruck und poröse Strömung wurden nicht nachgewiesen.
- Diffusionsflüsse sind volumen- und quellbegrenzte,
  sequentielle Modellflüsse; genaue Grenzschichtprofile,
  Konvektions- und Reaktionsfront-Konvergenz erfordern
  noch unabhängige Vergleiche.
- CPU-only, keine GPU-Beschleunigung, kein Renderer (P7/P8).

**Abschlussdefinition:** P6 ist als real ausführbarer,
eingeschränkt validierter 3D-CPU-MVP für definierte Stoffregeln,
atom- und massenerhaltende Umwandlung sowie Wärmebilanz
vollständig nachweisbar. Allgemeine Chemie bleibt ausdrücklich
außerhalb dieses abgeschlossenen Abnahmeumfangs.
