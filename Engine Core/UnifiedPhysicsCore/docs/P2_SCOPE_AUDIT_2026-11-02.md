# P2-Scope-Abgleich — 02.11.2026

**Ziel:** Ursprüngliches Phasenversprechen und später erweitertes
Voll-CFD-/FSI-Qualitätsziel sauber unterscheiden, ohne Anforderungen
nachträglich zu löschen oder einen künstlichen PASS zu erzeugen.

## Ursprüngliche P2 in PHASENPLAN.md (Stand 26.10.2026)

P2: echte volumetrische 3D-Wassertechnik erforschen und auswählen
(MAC / FLIP / APIC / MPM). Die damals festgelegte Abnahme umfasst:

1. C++17/MSVC-Release-Build und vollständige CTest-Regressionssuite.
2. 3D-MAC/VOF mit gemeinsamer Material-/Runtime-API.
3. Dammbruch auf drei Gittern, Volumenfehler unter 1e-8 m³.
4. Hydrostatik auf drei Gittern mit unter 1 % Referenzabweichung.
5. Stationäre 3D-Kollisions-/Hindernisprüfungen und begrenzter
   Wasser-Volumenanteil.
6. Dokumentierter Raster-/CPU-/Speichervergleich.
7. Maschinenlesbarer, reproduzierbarer Messbericht inklusive
   Quell-SHA256.

Am 02.11.2026 wurden nach aktuellem CMake-Build
`scripts/p2_acceptance.py` mit **30/30 nativen
quantitativen Prüfungen PASS** und CTest mit **146/146 PASS** erneut
ausgeführt. Das ist die nachvollziehbare **Abnahme des
ursprünglichen P2-CPU-Forschungs-/Prototyp-Umfangs**.

Im Gate wird `prototype_status` deshalb nur an diesen
P2-Referenznachweis plus CTest geknüpft. Die nachträglich
hinzugekommenen optionalen Viskositätsmodule sind nicht mehr
fälschlich Voraussetzung dieser ursprünglichen Auswahl-/
CPU-MVP-Abnahme. Deren unabhängige Quell-Hashprüfungen und
kritische fehlende Funktionsnachweise bleiben weiterhin als
eigene Teilprojekte und Warnhinweise erhalten.

## Später geforderter Gesamtumfang

`scripts/phase_gate.py` führt in `FULL_SCOPE_OPEN["P2"]`
weiterhin **unverändert** die drei offenen Kriterien:

- volle spannungskonsistente dreidimensionale Viskosität an
  bewegten freien Wasseroberflächen,
- dynamisches Fluid-Festkörper-Verhalten inklusive No-Slip-
  Kollision, Impulsreaktion und 3D-Hinderniskopplung,
- unabhängig gemessene 3D-Strömungs- und Impulsreferenzen.

Hinzu kommen in aktuellen technischen Forschungsprotokollen:
die noch nicht bestandene externe MARIN/SPHERIC
H1–H4- und P1–P8-Messkonvergenz, 6-Sekunden-Lauf,
Dreigitter-Nachweis, nicht konvergierender Kapillar-
Tropfen-Spurious-Velocity-Test und fehlender globaler
Oberflächenenergie-/Fluid-Festkörper-Impulsschluss.

Diese Aufgaben sind physikalisch erheblich umfangreicher
als die ursprüngliche MAC/FLIP/APIC/MPM-Auswahl.
Insbesondere gehört die vollständige **bidirektionale
Material-/Fluid-Kopplung** in der ursprünglichen Roadmap
auch zur Phase P3.

**Die aktuellen erweiterten FULL_SCOPE_OPEN-Kriterien werden
nicht entfernt.** Das offizielle Feld `status` bleibt
`PARTIAL`, solange die vollen Kriterien nicht unabhängig
PASS erreichen; `all_phases_complete=False`.

## Luft-Architektur

Im ersten Engine-Kern reichen eine explizite Wasserfüllung
und eine implizite Umgebungsphase mit atmosphärischem Druck.
Eine vollständige aktive Gas-VOF-/Luft-Momentum-Simulation
ist für die *ursprüngliche* P2-Auswahl nicht vorgeschrieben.
Die numerisch korrekte Wasseroberfläche bleibt aber nötig.
Reales Gas-/Dampf-/Wärmeverhalten kann optional integriert
werden, wenn konkrete Spieleffekte oder P4 dies erfordern.

**Kein neuer P2-Full-PASS aus diesem Scope-Abgleich.**
