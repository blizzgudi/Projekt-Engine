# P2 Teilabnahme: adaptives 3D-MAC/PLIC-CFL-Subcycling (31.10.2026)

**Status: PARTIAL, nicht P2-Vollabnahme.** Dieser isolierte Adapter
integriert die VOF-Wasserfuellung bei einem *vorgegebenen und waehrend
des Zeitschritts eingefrorenen* periodischen, diskret divergenzfreien
3D-MAC-Geschwindigkeitsfeld. Es gibt weder Navier-Stokes-Impulsupdate
noch Druckprojektion oder dynamische Fluid-Festkoerper-Rueckwirkung.

## Implementierung

- `include/upc/p2_plic_adaptive_subcycle3d.hpp` nutzt den bestehenden
  gleichzeitigen geometrischen 3D-PLIC-Flux-Operator. Er ermittelt die
  maximale absolute Sechs-Face-Courant-Summe ueber alle Gitterzellen,
  teilt `dt` in `ceil(CFL_max / CFL_target)` Teilintervalle und
  rekonstruiert die Grenzflaechennormalen nach jedem Teilintervall neu.
- Explizites Budget: `0 < CFL_target <= 1`, `1 <= max_substeps <= 4096`.
  Unmoegliche Budgets, ungueltige Geometrie/Zeiten, nichtsolenoidale
  Felder und vom Kernel erkannte ueberlappende Slabs werden abgelehnt.
  Es gibt keine heimliche Abschneidung des Zeitintervalls und keine
  kuenstliche Massenkorrektur.
- Read-only Eingaben, konservative Face-Deltas und aggregierte
  Laufzeitdiagnostik: Substep-Zahl, Courant, Divergenz, transferierte
  Faces und Gesamtvolumen. Der alte Einzelschritt-Operator bleibt
  unveraendert.

## Reproduzierbarer Nachweis

`scripts/p2_plic_adaptive_build.cmd` baut mit MSVC Release und
fuehrt die komplette CTest-Suite aus. Der eigenstaendige Native-Test
`tests/p2_plic_adaptive_subcycle3d_tests.cpp` prueft:

1. Bitidentitaet mit dem bestehenden Kernel bei einem einzigen Schritt.
2. Analytische periodische Translation eines 3D-Wasserquaders mit
   dreiachsiger Geschwindigkeit und CFL > 1 (inklusive Ecktransportfehler).
3. Analytische periodische Naht-Translation bei x-Grenzuebertritt.
4. Echte diskret solenoidale variable MAC-Curl-Geschwindigkeit, mehrere
   Teilschritte und erneute VOF-Normalenrekonstruktion.
5. Negative Eingaben (Zeit, CFL, Substep-Budget, Divergenz, Arraygroesse)
   und exakt unveraenderte stationaere VOF-Eingabe.

`scripts/p2_plic_adaptive_acceptance.py` prueft Native-Ausgabe,
105/105 CTests, die analytischen Referenzfehler, Bilanzgrenzen,
Buildprotokolle und SHA256-Hashes. Der maschinenlesbare Nachweis liegt
unter `ENGINE TEST STUBE/runs/p2_plic_adaptive_partial.json`.

## Grenzen

Der zeitliche Subcycle macht die geometrische unsplit-Slab-Summe
**nicht** raumzeitlich exakt. Der relative L1-Fehler der dreiachsigen
Quadertranslation ist messbar und wird nicht verschwiegen.
Die Schnittnormalen bleiben eine lokale Gradientenapproximation.
Weder freie Oberflaechenspannungen, 3D-Navier-Stokes-Dynamik,
gemessene externe CFD-Referenzen, bewegte Zweiwege-FSI,
globale physikalische Energie-/Impulserhaltung noch Unreal-5.8
Native-E2E sind mit diesem Teilnachweis abgenommen.
P2 bis P8 bleiben INCOMPLETE.
