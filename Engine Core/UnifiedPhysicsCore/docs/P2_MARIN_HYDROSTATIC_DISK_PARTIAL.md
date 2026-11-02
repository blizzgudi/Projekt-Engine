# P2 – MARIN Test-02: exakte hydrostatische Drucksensor-Scheibenreferenz (Teilnachweis)

**03.11.2026 – P2 PARTIAL / ALL_COMPLETE=False.** Keine Laborvalidierung und keine vollständige P2-Abnahme.

## Physikalischer Blocker und reproduzierter negativer Befund

Der vorhandene `P2MarinFootprintPressure3D`-Operator mittelt Zellmitten-Druck und
VOF-Zellfüllung auf einer hypothetischen kreisförmigen Drucksensorfläche.
Diese Volumenfüllung ist **nicht** die tatsächliche benetzte Sensorfläche.
Bereits eine einfache hydrostatische Referenz mit horizontaler Wasserlinie
zeigt eine große, **nicht monoton konvergierende** Abweichung am Dachsensor P5.

Geometrie: Original-Test-02-Box mit Dach `z=0.161 m`, Wasserlinie
`H=0.190 m`, `rho=1000 kg/m³`, `g=9.81 m/s²`. Ein hypothetischer
Diskradius `R=0.010 m` ist **nicht experimentell kalibriert**.
Der exakte hydrostatische P5-Flächendruck ist `rho*g*(H-z)=284.490 Pa`.

| Gitter | Native P5-Footprint [Pa] | Exakt [Pa] | Fehler [Pa] | Native benetzte VOF-Fraktion |
|---|---:|---:|---:|---:|
| 32×10×10 | 0.000 | 284.490 | -284.490 | 0.000 |
| 48×15×15 | 194.565 | 284.490 | -89.925 | 0.850 |
| 64×20×20 | 117.720 | 284.490 | -166.770 | 0.800 |

Der Druckfehler **steigt** vom mittleren zum feineren Gitter. Dies ist
ein konkreter, vom realen Sechssekunden-Laborfehler unabhängiger
Reproduktionsnachweis der unzureichenden Sensor-Abtastung. Es ist **kein**
CFD-Korrektur- oder Konvergenz-PASS.

## Implementierung und unabhängige Referenz

Neuer optionaler, analytischer `P2MarinHydrostaticDisk3D`-Operator.
Er berechnet die geometrisch exakt benetzte Kreisfläche und das
Integral von `rho*g*max(H-z,0)` über die **ganze** vertikale
P1–P4-Scheibe. P5–P8 werden auf der tatsächlichen horizontalen
Dachfläche bei `z=0.161 m` ausgewertet. Trockenanteile erhalten
korrekt Null-Gauge-Druck. Die Fläche wird nicht künstlich auf den
benetzten Rest renormiert. Die Druckkraft wird als positiver skalarer
Integralbetrag dokumentiert, **nicht** als vorgetäuschte 3D-FSI-Kraft.

Eine getrennte native Simpson-Streifenintegration mit 200.000
Unterteilungen pro Fall prüft die geschlossene Kreis-Segmentformel.
Bei einer halb benetzten P1-Scheibe gilt unabhängig:
`wet_fraction=0.5` und `p_disk=20.8175 Pa` für `R=0.01 m`.
Zehn Wasserlinien und alle acht Sensoren werden geprüft.
Die unabhängige Python-QA berechnet die aktuelle
Native-Zellmitten-Abtastung für die drei Test-02-Gitter direkt aus
`p(z_c)=rho*g*max(H-z_c,0)` und
`VOF=clamp((H-z_low)/dx,0,1)` neu.

## Native-/QA-Evidenz

- Erster Build ohne MSVC-Developer-Environment: **FAIL C1083**
  (Standardheader `algorithm` nicht gefunden); Buildumgebung
  anschließend mit `vcvars64.bat` korrekt eingerichtet.
- Erster nativer Geometrievergleich: **FAIL** wegen ungenügender
  Simpson-Diskretisierung an der scharfen Benetzungsgrenze
  (40.000 Streifen, 1.06e-5 Flächenabweichung bei Halbbenetzung).
  Unabhängige Integration auf 200.000 Streifen erhöht; Grenzwert
  `5e-6` für Flächenanteile, `0.02 Pa` für Druckintegrale.
- Danach **299 native Assertions PASS**, **156/156 CTest PASS**.
- Python-Audit mit separater Hydrostatik-/Zellmittelreferenz PASS;
  **12/12 manipulierte Reports abgewiesen** (Phasenflags,
  Nativezahlen, Geometrie, Gitternetze und SHA-256).
- SHA-256-Quellen-/Binary-/Build-/Native-/CTest- und historischer
  6-s-MARIN-Messreport-Nachweis in
  `ENGINE TEST STUBE/runs/p2_marin_hydrostatic_disk_partial.json`.
- Native Reproduktion: `scripts/p2_marin_hydrostatic_disk_native.cmd`;
  unabhängiges Audit:
  `scripts/p2_marin_hydrostatic_disk_audit.py`.
  Keine erneuten teuren Dreigitter-Sechssekundenläufe.

## Offene Akzeptanz

Die exakte Formel ist eine **Referenz für eine vorgeschriebene horizontale
Wasserlinie**, nicht der aktuelle native CFD-Druckprojektor und kein
experimentell aufgelöster Transducer. Eine nächste physikalische
Korrektur muss die tatsächliche 3D-Wasser/Luft-Schnittfläche und die
Wanddruck-Spannungsrandbedingung in die live Cut-Cell-Projektion und
flächenrichtige P1–P8-Abtastung koppeln. Dynamische bidirektionale FSI,
globale Masse-/Impuls-/Energiebilanzen und UE5.8-Native-E2E sind offen.

Historischer voller **6-s-MARIN/SPHERIC-Dreigitter-Laborvergleich
bleibt FAIL**: feine H1–H4-RMSE
`0.094788 / 0.100674 / 0.117963 / 0.137818 m`
gegen unveränderte `<=0.05 m`, P1–P8 ungefähr `5.8–5.9 kPa`.
Die read-only Gate-Prüfung meldet außerdem für den historischen
P2-Prototypbericht einen Source-Hash-Konflikt bei `CMakeLists.txt`
(nach der Testregistrierung); der alte Prototyp-PASS darf deshalb nicht
als aktueller sourcegebundener PASS ausgegeben werden.
Das echte `scripts/phase_gate.py` führt P2 wegen drei
offener vollständiger Akzeptanzkriterien weiter als **PARTIAL**.
Die bestehenden uncommitteten historischen Gate-/Report-Dateien
wurden nicht verworfen oder als neue PASS-Evidenz umgeschrieben.
