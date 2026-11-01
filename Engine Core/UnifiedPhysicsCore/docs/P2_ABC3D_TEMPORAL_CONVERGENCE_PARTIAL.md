# P2 Teilabnahme: zeitliche 3D-ABC-MAC/PLIC-Konvergenz

Stand: 01.11.2026. **P2 PARTIAL; P3–P8 INCOMPLETE.**

## Abgegrenzter Umfang

Die bestehende periodische, viskose 3D-MAC-Impulsadvektion mit Druckprojektion
und geometrischem PLIC-Wassertransport wurde gegen die exakte kontinuierliche
Arnold-Beltrami-Childress-(ABC-)Navier-Stokes-Lösung mit zeitlich wechselnder
kinematischer Viskosität geprüft. Bei gleicher physikalischer Endzeit von
0,1 s und identischem 12³-Gitter wird jeder der fünf ursprünglichen
Viskositäts-/Zeitintervalle in 1, 2 oder 4 Teilintervalle aufgeteilt.
Das ergibt **5, 10 und 20 echte gekoppelte Solver-Schritte**.

Die analytische Amplitude ist
`A(t) = A(0) exp(-4 pi² Integral(nu(t) dt))`.
Alle drei staggered Geschwindigkeitskomponenten sind ungleich null.
Die exakte Energiequote für das gewählte Feld ist 0,97512732142637137.
Die Wasserfüllgrade starten als ein 0,25-m³-Slab und werden in jedem Schritt
durch den echten PLIC-Transport weitergeführt. Die optionale
Endpunktbereinigung bleibt auf Rundungsfehler begrenzt.

## Reproduzierbarkeit

Im Verzeichnis `D:\Projekt Engine\Engine Core\UnifiedPhysicsCore`:

```bat
cmd /d /c scripts\p2_abc3d_temporal_build.cmd
python scripts\p2_abc3d_temporal_acceptance.py
python scripts\phase_gate.py
```

Der Build nutzt MSVC C++17 Release und den dauerhaft registrierten CTest.
Die quantitative Python-Teilabnahme prüft reale Native-/CTest-/Build-Protokolle,
die analytische Energiedämpfung, zeitliche Selbstkonvergenz, Wasserbilanz und
SHA256 der getesteten Quellen, des Native-Binaries und der Protokolle.
Nachweis: `ENGINE TEST STUBE/runs/p2_abc3d_temporal_partial.json`.

## Native-Messwerte

| Messung | 5 Schritte | 10 Schritte | 20 Schritte |
|---|---:|---:|---:|
| Relativer L2-Fehler gegen ABC | 0,0062348056125263252 | 0,0062232533147500652 | 0,0062174974334890691 |
| Numerische kinetische Energiequote | 0,96327611486810383 | 0,96329586991701766 | 0,96330573658271601 |

Die paarweisen L2-Geschwindigkeitsdifferenzen zwischen 5/10 und 10/20
Schritten sind **5,0491469690629016e-5** und
**2,4993526397911049e-5**. Ihr Quotient beträgt
**2,0201819017763367**, entsprechend ungefähr erster Ordnung
der zeitlichen Selbstkonvergenz bei festem Gitter. Der Fehler gegen die
exakte Lösung bleibt wegen räumlicher Diskretisierung und numerischer
Advektionsdissipation erheblich; er geht nicht auf null allein durch
Zeitverfeinerung. Die numerische Energiequote bleibt unter der
analytischen Quote; das ist **kein physikalischer Energieerhaltungsbeweis**.

Bei 20 Schritten: Wasservolumen 0,25000000000000022 m³,
Massenledger-Residuum -2,0407383462664562e-18 m³,
kumulierte absolute Endpunktkorrektur 1,7490963012879811e-17 m³,
maximale projizierte Divergenz 3,009880756091321e-15 s^-1.
Die Native-Assertions und die vollständige CTest-Zahl werden vom
Abnahmeskript ausschließlich aus den tatsächlichen Logs gelesen.

## Explizit offene Vollphasen-Kriterien

Das Ergebnis belegt numerische Zeitverfeinerung einer **konstante-Dichte,
periodischen, passiv wassertransportierenden** 3D-MAC/PLIC-Variante.
Es belegt keine spannungskonsistente Wasser-Luft-Grenzfläche, keine
dynamische Zweiwege-Fluid-Festkörper-Kopplung, keine unabhängigen
gemessenen 3D-CFD-Referenzen, keine vollständige globale physikalische
Energie-/Impulsbilanz und kein neues Unreal-5.8-Native-E2E.
**P2 bleibt offen; ebenso P3–P8.**
