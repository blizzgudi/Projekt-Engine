# P2 Teilfortschritt: PLIC-Unterzeitschritt-Rundung (01.11.2026)

**Status: P2 PARTIAL. Keine vollständige Phasenabnahme.**

## Nachgewiesener Fehler und Ursache

Im realen periodischen 16³-Galilei-ABC-Test mit Viskosität, nichtlinearer
MAC-Impulsfortschreibung, Druckprojektion und passivem PLIC-Wassertransport
brach der zweite von zwei geometrischen Transport-Teilzeitschritten mit
`Unresolved periodic PLIC interface` ab. Der erste Teilschritt schließt seine
Wasservolumenbilanz und erzeugt 515 teilgefüllte Zellen, von denen drei
Rundungsfüllgrade sehr nahe 0/1 mit unauflösbarem zentralen Gradienten besitzen.

Der neue opt-in `p2_plic_endpoint_subcycle3d` bereinigt **ausschließlich**
Füllwerte innerhalb von acht `double`-Maschinen-Epsilon von 0 oder 1
**nach jedem einzelnen PLIC-Teilzeitschritt**. Die absolute Korrektur muss
unter dem expliziten Volumenbudget bleiben. Keine Massenumverteilung, keine
erfundene Normale, kein Überschreiben ungültiger Eingaben. Ein wahrhaft
mehrdeutiger isolierter 0,5-Füllgrad wird weiterhin abgelehnt.

Die bestehende `p2_nonlinear_mac_plic_step3d`- und
`p2_viscous_nonlinear_mac_plic_step3d`-API behält den alten Pfad als
**Default**. Der zusätzliche letzte `bool`-Parameter aktiviert die
Korrektur ausdrücklich, inklusive diagnostisch ausgewiesener
`endpoint_snapped_cells`, signiertem und absolutem Korrekturvolumen.
Ein Fehler liefert keinen teilweise übernommenen Simulationszustand.

## Native Nachweise

`tests/p2_plic_substep_trace3d_tests.cpp` rekonstruiert die physikalischen
ABC-Geschwindigkeiten einschließlich des Viskositätsstencils und der
nichtlinearen Druckprojektion auf 8³, 12³ und 16³. Der instrumentierte,
unveränderte PLIC-Kernel wird pro Teilzeitschritt erneut ausgeführt;
als Gegenprobe wird der bisherige echte Solver auf denselben Zustand
angewendet.

Vorläufiger 16³-Befund:

- Original: 2 Teilzeitschritte geplant, erster bestanden, zweiter
  mit `Unresolved periodic PLIC interface` fehlgeschlagen.
- Vor dem Fehler: 515 teilgefüllte Zellen, drei nahe den
  Füllgrenzen unauflösbar, keine echte geometrische Mehrdeutigkeit.
- Opt-in: beide Teilschritte bestanden; sieben Korrekturen und
  nur ca. `1.8973538018496328e-19 m³` absolutes Korrekturvolumen.
- Echte viscous/nonlinear-MAC/PLIC-Integration mit Opt-in stimmt bei
  Anzahl und Volumen der Korrekturen mit dem isolierten Replay überein.
- Wasser bleibt bei ca. `0.5 m³`; Druckprojektion divergenzfrei
  innerhalb des numerischen Testlimits.
- Synthetische echte Mehrdeutigkeit wird auch bei aktiviertem Opt-in
  zurückgewiesen.

Reproduktion aus `Engine Core/UnifiedPhysicsCore`:

```bat
cmd /d /c scripts\p2_plic_substep_fix_build.cmd
python scripts\p2_plic_substep_fix_acceptance.py
python scripts\phase_gate.py
```

Protokolle und maschinenlesbare Teilabnahme:
`ENGINE TEST STUBE/runs/p2_plic_substep_fix_*.txt` und
`ENGINE TEST STUBE/runs/p2_plic_substep_fix_partial.json`.
Das bestehende CTest-Regressionspaket wird separat vollständig ausgeführt.

## Grenzen

Dies ist ein **abgegrenzter numerischer P2-Fehlerfix**, noch keine
spannungsphysikalisch korrekte 3D-Zweiphasenströmung. Es wird nicht behauptet,
dass beliebige 16³-Mehrschrittbedingungen oder große Netzauflösungen bereits
bestehen. Reale dynamische Zweiwege-FSI, freie Wasseroberflächen mit
Stressrandbedingungen, externe CFD-Messreferenzen, globale Energie- und
Impulserhaltung, Unreal-5.8-Native-E2E des neuen Pfades und P3–P8-Vollabnahmen
bleiben offen. `ALL_COMPLETE=False`.
