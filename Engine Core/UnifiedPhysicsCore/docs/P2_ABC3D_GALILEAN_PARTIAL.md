# P2 Teilabnahme: Galilei-verschobene 3D-ABC-Navier-Stokes-Welle

Stand: 01.11.2026. **P2 PARTIAL; P3–P8 weiterhin offen.**

## Physikalischer Umfang

Die vorherige stationäre ABC-Referenz prüfte eine Beltrami-Strömung, deren
nichtlineare Advektion vollständig als Druckgradient absorbiert wird. Ein
falscher oder fehlender Transport des Strömungsfelds könnte deshalb teilweise
unentdeckt bleiben. Der neue Test ergänzt eine **exakte bewegte Referenz**:

`u(x,t) = U + A exp(-nu (2 pi)^2 t) F(x-U t)`,
mit `U=(0.25,-0.15,0.10) m/s`, `A=0.05 m/s`,
`nu=0.003 m²/s`, `t=0.02 s` und
`F=(sin(z)+cos(y), sin(x)+cos(z), sin(y)+cos(x))`
für periodische Koordinaten in Metern. Die exakte Lösung folgt aus
`curl(F)=2 pi F`, `div(F)=0`; der Druck absorbiert den
Beltrami-Gradienten. Die Translation mit `U` fordert eine echte
nichtlineare Impulsadvektion und ist **nicht** mit der stationären
ABC-Referenz identisch.

Auf 8³, 10³ und 12³ Gittern wurde jeweils ein realer gekoppelter
3D-MAC-Viskositäts-, Impuls-, Druckprojektions- und PLIC-Transport-
Schritt ausgeführt. Die Anfangs-VOF ist ein halb gefüllter Wasser-Slab
mit 0,5 m³; die Flüssigkeit wird im Solver **passiv** transportiert,
nicht mit physikalischer Wasser-Luft-Spannung gekoppelt.

## Native Resultate

| Größe | 8³ | 10³ | 12³ |
|---|---:|---:|---:|
| Relativer L2-Fehler zur bewegten Lösung | 0.0084743446192049442 | 0.0067884174107725494 | 0.0056416436096240578 |
| Relativer L2-Fehler zur falschen stationären Lösung | 0.021842115930483796 | 0.022036151068170331 | 0.022138933039594806 |
| Maximale Abweichung des periodischen Mittelwerts (m/s) | 2.78e-17 | 2.08e-16 | 2.22e-16 |
| Numerische Energiequote der Geschwindigkeitsfluktuation | 0.98043207074790573 | 0.98323198498125752 | 0.98519515508300681 |
| Wasservolumen (m³) | 0.49999999999999994 | 0.50000000000000022 | 0.49999999999999978 |

Die unabhängige analytische Energiequote ist
`exp(-2 nu (2 pi)^2 t)=0.99527379371540581`.
Der Abstand zwischen numerischer und analytischer Energie zeigt
**zusätzliche numerische Dissipation**. Der relative L2-Fehler sinkt
mit Gitterverfeinerung. Die bewegte analytische Lösung passt deutlich
besser als die unverschobene Vergleichslösung; die drei Komponenten
der mittleren Strömung bleiben bis auf Rundungsfehler erhalten.

Die 12³-Divergenz betrug 9,82e-15 s^-1, das Wasserbilanzresiduum
-7,39e-18 m³. Der Test meldete 12.987 native Assertions und die
vollständige CTest-Suite 109/109 PASS.

## Reproduzierbarkeit und Einschränkungen

```bat
cd /d "D:\Projekt Engine\Engine Core\UnifiedPhysicsCore"
cmd /d /c scripts\p2_abc3d_galilean_build.cmd
python scripts\p2_abc3d_galilean_acceptance.py
python scripts\phase_gate.py
```

Die maschinenlesbare Teilabnahme liegt unter
`ENGINE TEST STUBE/runs/p2_abc3d_galilean_partial.json`, zusammen mit
den realen Build-, Native- und CTest-Protokollen und SHA256-Fingerprints.

**Zusätzlicher echter Negativbefund:** Ein erster Lauf mit 16³ und
bewegtem ABC-Feld brach im geometrischen PLIC-Pfad mit
`Unresolved periodic PLIC interface` ab. Der Fehler trat sowohl
mit einem schmalen als auch mit einem breiten Wasser-Slab und bei
vollständig gefülltem VOF-Gitter auf. Der 16³-Fall wird **nicht**
als bestanden gewertet; die interne PLIC-Stufen-Rundung und
Grenzflächennormalenrekonstruktion sind separat zu untersuchen.
Das erfolgreiche 8³–12³-Experiment verschweigt diese Grenze nicht.

**Keine Vollphasenfreigabe:** periodische konstante-Dichte-Strömung,
passiver VOF-Füllgrad, keine stresskonsistente freie Oberfläche, keine
dynamische Zweiwege-FSI, keine externen gemessenen 3D-CFD-Daten,
keine globale physikalische Energie-/Impulsabnahme und kein neues
UE5.8-Native-E2E. P2–P8 bleiben unvollständig.
