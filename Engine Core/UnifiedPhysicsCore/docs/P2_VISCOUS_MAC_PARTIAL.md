# P2-Fortschritt – 3D-MAC-Viskositätsoperator (Teilnachweis)

Stand: 2026-10-26. Der ursprüngliche P2-MAC/VOF-CPU-MVP wird um einen
**optionalen, getrennten Viskositätsoperator** ergänzt. Dies ist KEIN
vollständiger P2-Abschluss und ändert weder den P8-Unreal-Adapter noch
die bisherigen Wasserergebnisse.

## Tatsächliche Implementierung

- \`include/upc/viscous_mac3d.hpp\`: \`ViscousMacDiffusion3D\` diffundiert
  die x/y/z-Geschwindigkeiten eines existierenden \`MacProjection3D\`-
  Gitters mit einer expliziten 6-Nachbar-Laplace-Diskretisierung:
  \`v_next = v + (nu*dt/dx^2) * (sum(neighbors)-6*v)\`.
- Stabile Teilzeitschritte bei \`nu*dt_sub/dx^2 <= 0.15\`, Maximum
  10000 Teilzeitschritte. Ungültige Gitter/Parameter und nichtendliche
  Werte werden zurückgewiesen.
- Ruhende äußere Randbedingungen: Null-Ghost für tangentiale
  Geschwindigkeiten, exakt null auf den äußeren Normal-Faces.
- \`nu=0\` ist ein No-op und erhält das bisherige inviscide Verhalten.
- Der \`face_energy_proxy\` ist die Summe der quadrierten
  Face-Geschwindigkeiten mit Zellvolumenfaktor, **keine** vollständig
  physikalische kinetische Energie des Kontinuums.
- \`tests/viscous_mac3d_tests.cpp\`: Vergleich gegen einen unabhängigen
  analytischen Eigenwert des diskreten 3D-Laplace-Operators, Abnahme
  des Energie-Proxys, isotrope x/y/z-Impulsdiffusion, große Zeitschritte,
  Null-Viskosität und ungültige Eingaben.
- CMake/CTest integriert unter \`physics_p2_viscous_mac3d_analytic_reference\`.

## Tatsächlich gemessener MSVC-Release-Nachweis

- 1174 Assertions; maximale Eigenmode-Abweichung 2.22e-16
  (Schwelle 1e-12).
- Energie-Proxy: 0.07425 -> 0.072814774756457 (erster Schritt);
  nach 61 Schritten 0.022573695158372.
- Stabile Teilzeitschritte bei dt=0.2 s, nu=1 m²/s, dx=0.1 m:
  134 Substeps. Impulsausbreitung in allen drei Achsen gleich.
- CTest: 33/33 PASS (voller Regressionstest inklusive P1–P8).
- \`scripts/p2_viscosity_acceptance.py\` erstellt
  \`ENGINE TEST STUBE/runs/p2_viscosity_partial.json\` mit
  numerischen Kriterien und Quellhashes.

## Explizit NICHT erreicht

- Der Viskositätsoperator ist **noch nicht** in
  \`CoupledMacWater3D::substep\` eingebunden. Er ist ein isoliertes
  MAC-Geschwindigkeitsmodul, kein vollständig gekoppelter
  Flüssigkeits-Viskositätssolver.
- Kein physikalisch korrekter Viskositäts-Randfluss an
  Wasser-Luft-Grenzen, kein bewegtes Festkörper-No-Slip,
  kein Mehrphasen-Viskositätskontrast und keine experimentelle
  Validierung eines realen 3D-Dammbruchs.
- Kein Nachweis einer globalen Impuls-/Energieerhaltung bei
  P2/P3/P4-Kopplung, kein vollständiger AAA-CFD-Solver.

**P2-Vollstatus: PARTIAL.** Vor Vollabnahme mindestens:
gekoppelter Viskositätspfad mit Flüssigkeitsoberfläche und
Festkörpern, externe 3D-Referenz/Validierung sowie
Druck-/Impuls-/Massen-/Zeitbudget-Nachweise.


## Zweiter Teilnachweis: Maskierte 3D-Viskosität (2026-10-26)

**Neue echte Implementierung:** Die bereits existierende Klasse
\`ViscousMacDiffusion3D\` besitzt jetzt zusätzlich \`step_masked(...)\`
mit expliziten 3D-\`wet\`- und \`solid\`-Zellmasken. Diese Funktion ist
bewusst weiterhin ein **separater Operator** und noch kein
gekoppelter \`CoupledMacWater3D\`-Solver.

Die drei Richtungen der versetzten MAC-Faces unterscheiden:
- Außenwände und stationäre feste Hindernisse: Normalgeschwindigkeit 0.
  Nachbarn dieser Faces verwenden Dirichlet-Null (vereinfachtes No-Slip).
- Aktive Wasser- oder Wasser-Luft-Faces: 3D-Laplace-Diffusion.
- Luft-Luft-Faces: Geschwindigkeit 0; beim Sampling von einem aktiven
  Wasserface über eine Luftgrenze: Nullgradient (Neumann-Proxy).
- Stabilität: explizite adaptive Substeps bei \`nu*dt_sub/dx²<=0.15\`,
  Ablehnung überlappender Masken und nichtendlicher Parameter.
- \`nu=0\` lässt die Eingabe einschließlich vorhandener Faces exakt
  unverändert, sodass alte inviscide Tests nicht beeinflusst werden.

\`tests/viscous_mac3d_masked_tests.cpp\` prüft echte 3D-Faces:
stationäre feste Innenzelle, geschlossene Domainwände, isolierte
Luft-Luft-Faces, aktive Flüssigkeitsdiffusion, 30 dissipative Schritte,
analytisch unabhängigen diskreten Wasser-Luft-Stencil (\`1-alpha\`),
gleichförmige Bulk-Geschwindigkeit, Null-Viskosität, ungültige
Eingaben und adaptiven großen Zeitschritt. Das Abnahmeskript
\`scripts/p2_viscosity_acceptance.py\` prüft die Messwerte beider
nativen Testprogramme und speichert SHA256 aller Quellen.

**Einschränkungen:** Neumann am freien Rand ist ein einfacher
numerischer Nullgradient, kein nachgewiesener vollständiger
spannungsfreier Grenzflächen-Tensor. Tangentiales No-Slip an beliebigen
Voxeloberflächen ist nur approximiert; bewegliche Hindernisse,
Impulsrückgabe an Körper, realistische Oberflächenspannung,
gekoppelte Druckprojektion mit viskoser Advektion und unabhängige
3D-CFD-Messdaten fehlen. **P2 bleibt PARTIAL.**

## Nachtrag (26.10.2026): Live-Kopplung separat nachgewiesen

Die obigen Grenzen beschreiben den **historischen isolierten Operator**
zum Zeitpunkt seiner ersten Abnahme. Inzwischen gibt es zusätzlich
einen optionalen Live-Pfad im `CoupledMacWater3D`-Solver.
Seine Tests, Messwerte und weiter offenen Grenzen sind in
`docs/P2_LIVE_VISCOUS_MAC_PARTIAL.md` und
`ENGINE TEST STUBE/runs/p2_live_viscosity_partial.json`
separat dokumentiert. Die vollständige P2-Phase bleibt **PARTIAL**.
