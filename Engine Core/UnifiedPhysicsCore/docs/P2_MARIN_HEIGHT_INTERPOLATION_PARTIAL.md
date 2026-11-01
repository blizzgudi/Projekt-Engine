# P2 Teilnachweis: subzelluläre H1–H4-Wasserstandssonden, echte 3D-MAC/PCG-Daten

Stand: 01.11.2026. **P2 INCOMPLETE; physikalische MARIN-Abnahme FAIL.**
Dieser Schritt verändert ausschließlich die *optionale Auslesung* der
vier Wasserstandssonden. Weder Druckprojektion, 3D-VOF-Transport noch
freie Oberflächen, Wandtraktion oder bewegte Körper wurden korrigiert.

## Implementierung und Native-Ausführung

- include/upc/p2_marin_height_interp3d.hpp: bilineare, auf 1 normierte
  x/y-Interpolation des vertikal über echte VOF-Füllgrade integrierten
  Wasserstands. H1–H4 bei x=0,496/0,992/1,488/2,632 m, y=0,5 m.
  Ungültige Zellen, NaN, Überfüllung und Sonden außerhalb der Domäne
  werden abgewiesen; Randsonden nutzen die nächste Zellmitte.
- benchmarks/p2_marin_dambreak_heights.cpp: optionaler letzter
  Modusparameter height_interp; der Legacy-Modus und die vorhandene
  6-s-Trajektorie bleiben unverändert. Das neue native Format
  kennzeichnet height_sampling=bilinear_column.
- tests/p2_marin_height_interp3d_tests.cpp: **60 native Assertions**,
  analytisch exakte affine VOF-Felder auf 32×10×10 und 64×20×20,
  schrumpfender quadratischer Interpolationsfehler bei Halbierung
  von dx, trockene/volle Spalten, Randfälle, Negativfälle und echte
  Live-MAC-Wasserfelder auf beiden Gittern.
- **Ein echter neuer 200-Schritt-Lauf** bis 2,00 s auf 32×10×10
  (dx=0,10 m), mit 3D-MAC/VOF-Throughflow, PCG-Projektion,
  unveränderten P1–P8-Druckproxys und neuen H1–H4-Sonden.
  Der bereits vorhandene PCG-200-Schritt-Lauf ist die unveränderte
  numerische Kontrollgruppe.
- **118/118 CTest-Suites PASS** (MSVC Release), **6/6** unabhängige
  Python-Regressionsfälle PASS, darunter sechs gezielt korrumpierte
  Trace-Varianten. Numerische Integrität ist kein experimenteller
  CFD-PASS.

## Original-MARIN/SPHERIC-Test-02: echte Messwerte, keine Kalibrierung

Originalarchiv-SHA256:
a2ad06e367f61c70351f3d66a8dc5041b53972e3a3aecebde0e06efdd62bf6d1.
Die 200 simulierten Zeitpunkte wurden gegen die Original-Labordaten
interpoliert. Alle vier RMSE verletzen das vorherige Limit von 0,05 m.
Drei Sonden werden geringfügig besser, H2 schlechter.

| Sonde | Alte nächstliegende Spalte, RMSE (m) | Neue bilineare Spalte, RMSE (m) | Befund |
|---|---:|---:|---|
| H1 | 0,120675591 | 0,120271258 | Verbesserung, FAIL |
| H2 | 0,117172003 | 0,120037144 | Verschlechterung, FAIL |
| H3 | 0,161974586 | 0,160737044 | Verbesserung, FAIL |
| H4 | 0,205689883 | 0,202500888 | Verbesserung, FAIL |

Der maximale Unterschied der neuen zur alten Wasserstandsauslesung
beträgt H1/H2/H3/H4: 0,001808 / 0,009356 / 0,007447 /
0,004441 m. **Alle** anderen 12 numerischen Signale (Zeit,
Wasservolumen, MAC-Divergenz, P1–P8 und Anzahl nasser Drucksonden)
sind für jeden der 201 Zustände exakt identisch (max. Differenz 0).
Das belegt eine reine Ausleseänderung ohne geänderte Fluidphysik.

Die acht P1–P8-Druck-RMSE bleiben zwischen 4,15 und 4,87 kPa
für diese 2-s-Trajektorie; der unveränderte Wanddruck-Proxy ist
keine physikalisch validierte Transducerflächen-Traktion.

## Native Artefakte und SHA-256

| Datei | SHA-256 |
|---|---|
| ENGINE TEST STUBE/runs/p2_marin_height_interp_coarse200.txt | e4a77a1947740c38b3cf1e8a7738f75af0d494b915f674216159d3d7ea8bbbc5 |
| Bisheriger PCG-200-Step-Trace | b7858667cc945bafa5f56356628df910a3f261c00a994781c7c12d3ea564c304 |
| build/p2_marin_dambreak_heights.exe | c675beefc448699763fac9ccfe1a1f382f0b8a7f55f106d6d181ed8320b4648c |
| build/upc_p2_marin_height_interp_tests.exe | 9c37b77dad4025b7475e39a25de900b70f42a7b07cd0ba21b93c8f70ad3c83fc |
| Native-Testlog | 2ab4e3da26b08091bdf9cecf7a80f227284e03ddc5f421b68fca38c13be19311 |
| CTest-118-Log | 3e1a8e5a33bb6322468fff256a4fd1c713a42fe4060e89f226ec27b9951e6dc3 |
| Python-QA-6-Log | 8fe476c78198bf4e899e54ad90776b771e4bff48caf36c2c39329950cbacec3f |
| Maschinenlesbarer Teilbericht | f55ab8e6b3e77147138bc6ab4c1fa933ecde527cf22ee1dac0b02c210063b176 |

Der Bericht ENGINE TEST STUBE/runs/p2_marin_height_interp_partial.json
enthält zusätzlich SHA256 aller sieben beteiligten Quelldateien und
den Vergleich jeder Messreihe. Alle Artefakte bleiben unter D:\Projekt Engine.

## Reproduktion und Grenzen

- Ohne teuren Neulauf: scripts\p2_marin_height_interp_build.cmd
  prüft die vorhandenen Rohdaten, Logdateien und Hashes.
- Vollständige Reproduktion mit Neubau, 200 echten Zeitschritten,
  nativen Tests und Vergleich: scripts\p2_marin_height_interp_build.cmd --run.
  Der neue --run-Batchpfad wurde bereitgestellt, aber **nicht nochmals
  vollständig durchlaufen**; die Einzelkommandos wurden nativ ausgeführt.
- Die bilineare VOF-Spaltenhöhe ist bei überschlagenden, spritzenden
  oder luftdurchsetzten Oberflächen **keine eindeutig messbare freie
  Oberflächenhöhe**. Sie modelliert weder Sondeninstrument-Dynamik
  noch einen endlichen Mess-Footprint.
- Das 2-s-Gitterexperiment ersetzt weder den vorhandenen 6-s-Grobgitterlauf
  noch ein unabhängiges 6-s-Feingitter und drittes Konvergenzgitter.
  Druck-Footprints, stresskonsistente 3D-Wasser-Luft-Grenze,
  dynamische Zweiwege-FSI und globale physikalische
  Massen-/Impuls-/Energiebilanzen bleiben offen.

**P2 bleibt PARTIAL; phase_gate.py darf P2 nicht auf PASS setzen.**

Der abschließende echte Aufruf von scripts/phase_gate.py ergab
GATES_PASS=P0, GATES_INCOMPLETE=P1,P2,P3,P4,P5,P6,P7,P8 und
ALL_COMPLETE=False. P1 ist in diesem neuen Gate **nur wegen der
veralteten CMakeLists.txt-SHA im älteren P1-Evidenzbericht** als
PARTIAL ausgewiesen; das ist kein neuer physikalischer P1-Testfehler.
Die Evidenz wurde nicht künstlich auf PASS umgeschrieben.
