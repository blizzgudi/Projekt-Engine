# P2 Teilnachweis: statische 3D-Mehrvoxel-Druckkraft und Drehmoment

Stand: 27.10.2026. **P2 PARTIAL; keine vollständige Phasenabnahme.**

## Implementierung

`include/upc/static_pressure_load3d.hpp` erweitert den bisherigen
nur lesenden statischen MAC-Drucksensor um
`StaticSolidPressureProbe3D::load_on_body(body_mask, torque_origin_m)`.
Eine explizite binäre Körpermaske wählt eine oder mehrere Festkörperzellen
aus; äußere feste Domänenwände gehören nicht automatisch zum Körper.
Jede benetzte, außenliegende Körperfläche liefert die Kraft
`dF = -p_fluid_center * n_solid_to_liquid * dx^2` in N und das
Drehmoment `dM = (face_center_m - torque_origin_m) cross dF` in Nm.

Die Abfrage validiert Maskengröße, Teilmenge der Festkörpermaske,
Nichtleere, endlichen Drehmoment-Ursprung und endliche Ergebniswerte.
Die zugrunde liegende MAC-Projektion, Wasser-/Solid-Zustände und
Geschwindigkeiten bleiben unverändert. Das Verfahren ist eine
**statische Druckflächendiagnose**, kein dynamischer FSI-Kraftaustausch.

## Unabhängige analytische Referenz

`tests/p2_static_body_wrench3d_tests.cpp` verwendet eine echte
3D-MAC-Projektion für einen vollständig benetzten 2x2x2-Solid-Block
(8 Voxel, 24 äußere benetzte Flächen), mit 3 Gittergrößen
8x7x6, 12x11x10 und 16x15x14. Die Umgebung ist entweder
offen zur Luft (Dirichlet) oder geschlossen (Neumann, Druck-Gauge).
Zwei Druck-Offsets 150/1200 Pa und ein linearer Druckgradient
`(7,-11,5) Pa/Zelle` liefern 12 echte native Szenarien.

Für `dx=0.08 m` lautet die **unabhängige** exakte diskrete Referenz
für die vollständig benetzte Würfeloberfläche:
`F = -12 * dx^2 * (7,-11,5) = (-0.5376,+0.8448,-0.3840) N`.
Für das Körperzentrum `r_c=((ox+1),(oy+1),(oz+1))*dx` gilt
`M_origin = r_c cross F`; um das Zentrum ist `M_center = 0`.
Die zweite unabhängige Python-Implementierung in
`scripts/p2_static_body_wrench_acceptance.py` prüft Kräfte,
Drehmomente, Ursprungstransformation, Gauge-Invarianz,
Divergenz, 3 Auflösungen und beide Randfälle.

Negativtests: ungültige/nichtbinäre/leere Körpermaske, Auswahl
einer Fluidzelle, falsche Maskengröße, NaN-Ursprung und trockene
Körperoberfläche. Vergleich mit der Summe der acht individuellen
statischen Voxel-Druckkräfte.

## Reproduktion / Evidenz

- MSVC Release: `cmake --build build --config Release --parallel 4`
- Gesamttests: `ctest --test-dir build -C Release --output-on-failure`
- Unabhängige Teilabnahme:
  `python scripts/p2_static_body_wrench_acceptance.py`
- Report: `ENGINE TEST STUBE/runs/p2_static_body_wrench_partial.json`
- Vollständiges CTest-Log:
  `ENGINE TEST STUBE/runs/p2_static_body_wrench_ctest_20261027.txt`
- Quell-/Binary-/Log-SHA256 im Report, explizit
  `partial_accepted=true` nur bei allen Prüfungen und
  `phase_complete=false`.

## Gemessene Teilabnahme vom 27.10.2026

- MSVC Release Build erfolgreich; **42/42 CTest PASS**.
- 12 native MAC-Projektionsszenarien, **11.768 Assertions PASS**.
- Unabhängiges Python-Gate: **188/188 Checks PASS**.
- Maximale Druckkraftabweichung der 12 Szenarien:
  **1.74e-8 N**, maximales Drehmomentresiduum:
  **9.59e-9 Nm**, Restdivergenz unter **9.32e-11 1/s**.
- P1 nach CMake-Erweiterung erneut mit **33/33 Checks PASS**;
  frühere P2-Manufactured-Projection und statische Einzelvoxel-
  Kraftreferenz mit **56/56** bzw. **140/140 Checks PASS**.
- Quell-, Release-Binary- und vollständige CTest-Log-SHA256 werden
  im maschinenlesbaren Report erfasst. **P2 ist nicht abgeschlossen.**

## Grenzen

Die Kraft-/Drehmomentreferenz ist eine diskrete Manufactured Solution,
**keine unabhängige gemessene 3D-CFD-Referenz**. Keine dynamische
Geometrie, keine Impulsrückwirkung auf Wasser, keine viskose
Schubspannung oder Oberflächenspannung, keine gesicherte physikalische
Gesamtenergie, keine Kopplung mit P3–P6 und keine neue
Unreal-5.8-Native-E2E-Abnahme. Druck-Gauge-Invarianz ist nur für die
hier vollständig benetzte, geschlossene Oberfläche beansprucht.
P2–P8 bleiben in der Vollphasenprüfung offen.
