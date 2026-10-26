# P4 Thermodynamik / Phasenwechsel – 3D-CPU-MVP-Abnahme

Stand: 2026-10-26. Projektwurzel: D:\Projekt Engine.

**Abnahmeumfang:** 3D-Wärmeleitung, Enthalpie und Eis/Wasser/Dampf,
konservative Kopplung an Voxel-/Material-IDs sowie MAC-3D-Volumenfluss und
P3-poröse Bodenmaterialien. Ein validierter Echtzeit-Mehrphasen-CFD-
oder Gassolver ist damit ausdrücklich NICHT vorhanden.

## Realisierte Systeme

1. **ThermalVolume3D:** Konservativer sechsflächiger 3D-Fourier-
   Wärmefluss Q = k_harmonic*(dx²/dx)*(Ta-Tb)*dt. Isolierte Ränder,
   adaptive Zeitschritte und paarweise gleiche/entgegengesetzte
   Wärmetransfers. Extrem kleine VOF-Zellfragmente erhalten eine
   konservative begrenzte Zweizellen-Relaxation statt Millionen
   expliziter Teilzeitschritte. Präzision der Begrenzung ist
   noch kein wissenschaftlich validierter subzellularer Heat-Solver.
2. **Phasen-Enthalpie:** Wasser bei ungefähr 101325 Pa mit
   Schmelz-/Siedeplateaus von 273,15/373,15 K, latenter
   Schmelzwärme 334 kJ/kg und Verdampfungswärme 2256 kJ/kg.
   Eis-, Wasser- und Dampfmassen bleiben in Summe erhalten;
   Heiz-, Schmelz-, Siede-, Kondensations- und Gefrierrunden sind geprüft.
3. **ThermalPhaseMacBridge3D:** Spiegelung von Phasenänderungen
   in das bestehende 3D-MAC-VOF-Raster. Vollständig gefrorene
   Wasserzellen sind Hindernisse. Beim Auftauen wird der
   Flüssigkeitsanteil wieder freigegeben. Gasmasse wird als
   *nicht strömendes* Inventar gespeichert, nicht als flüssiges MAC-Wasser.
   Feste Materialvoxel erhalten aktualisierte Temperaturen.
4. **ThermalMacAdvection3D:** Direkte Rückkopplung an dieselben
   konservativen MAC-Flüsse des 3D-Wassers. Bei jedem Zelltransfer
   werden Wassermasse und Enthalpie zusammen transportiert und
   nachträglich mit MAC-Volumina abgeglichen. Teilgefrorene oder
   gasreiche Mischungen werden für diese Advektionsmethode
   explizit abgewiesen und nicht stillschweigend falsch transportiert.
5. **ThermalPoreExchange3D:** In P3 dringt Wasser aus der
   freien Flüssigkeit in Material-ID-gebundene poröse Bodenvoxel
   ein. Dasselbe Wasservolumen trägt seine spezifische Enthalpie
   ins Porenkonto. Beim Abwärtsfluss wird der Verlust an
   Gravitationspotential rho*g*V*dz in dieses Konto als Wärme
   übertragen. Die Bilanz Thermalenergie + Gravitationspotential
   ist während des *Porentransfers* erhalten. Ein 3D-Integrationstest
   kombiniert MAC-Advektion, Porenfluss und Wärmeleitung.
6. **Volumendiagnose:** Wasser-/Eis-/Dampf-Volumen auf Basis
   1000/917 kg/m³ sowie ideales Dampfgas pV=mRT
   (R=461,5 J/(kg K)). Diese Bilanz dient zur
   Fehlerdiagnose; das tatsächliche Dampfvolumen verdrängt
   noch keine benachbarten Gitterzellen.

## Abnahmekriterien / Prüfbeweise

- Native MSVC C++17 Release-Build und alle CTest-Suites PASS.
- 3D-Fourier-Wärmeleitung gegen analytische diskrete
  Neumann-Eigenmode bei 8³, 12³, 16³ und 24³:
  Profilabweichung <0,05 K und relativer Energiefehler <1e-10.
- Einzelne Wärmequellen, absolute Kelvin-Grenze und
  geschlossene Enthalpie-Rundreise beim Eis/Wasser/Dampf-Wechsel.
- Festkörper/Wasser-Wärmekontakt und temperaturaktualisierte Voxel.
- MAC-Flüssigkeitsbewegung in 3D transportiert Wasser
  einschließlich Enthalpie ohne messbare Masse-/Wärmeverluste.
- MAC/Eis/Dampf-Fraktionsübergänge bewahren sämtliche Wassermassen.
- P3-Bodenporen erhalten Wärme aus transportierter Flüssigkeit
  und aus abgebautem Gravitationspotential, ohne zusätzliche
  künstliche Masse oder Energie bei den isolierten Austauschschritten.
- Reproduzierbarer Performancewert für Fourier-3D-Benchmarks,
  native Regressionen und SHA256-Fingerprints zu allen P4-Quellen.
- Gesamtabnahme über scripts/p4_acceptance.py; Maschinenbericht:
  ENGINE TEST STUBE/runs/p4_acceptance.json.
  Projektstatus: scripts/phase_gate.py.

## Physikalische Grenzen der P4-CPU-MVP

- Kein vollständiger Dampf-/Gasdruck- oder Expansionssolver,
  keine atmosphärische Dampfkonvektion oder Dampfvolumen-Kollision.
  Gefrierdehnung und Kräfte aus Eisbildung nur diagnostiziert.
- Teilgefrorene Zellen sind noch keine geometrisch aufgelöste
  Eis-/Flüssigkeitsgrenze. Wasservolumen als MAC-Anteil bleibt
  für liquid bei seiner nominalen Dichte.
- Keine full-physics Konvektions-/Latentheat-Mehrphasenströmung,
  keine druckabhängigen Dampftafeln oder Salzgehaltsmodelle.
- Porenwasser und trockenes Bodenmaterial werden separat
  bilanziert; Wärmeaustausch zwischen beiden innerhalb EINER
  Bodenzelle ist noch nicht als konstitutives Modell gelöst.
- Vollständige mechanische Energie der turbulenten MAC-
  Wasserbewegung und Kräfte aus P3 werden NICHT als Wärme
  zurückgeführt. Die geprüfte Energieerhaltung umfasst
  Fourier-Leitung, liquide Enthalpieadvektion und lokale
  P3-Darcy-Gravitationsenergie, nicht ein vollständig
  geschlossenes Gesamtsystem aller mechanischen Freiheitsgrade.
- Keine Echtzeit-GPU-Qualifikation (P7), keine visuelle
  Unreal-Integration (P8) und keine Realfluid-Validierung.

**P4-PASS bedeutet den verifizierten abgegrenzten Thermodynamik-
CPU-Prototyp, nicht eine vollständig entwickelte Multiphysik-Engine.**
