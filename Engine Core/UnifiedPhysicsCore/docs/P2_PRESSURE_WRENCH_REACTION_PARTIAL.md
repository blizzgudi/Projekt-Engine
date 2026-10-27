# P2 Teilnachweis: angewandte sechsachsige MAC-Face-Druckreaktion

Stand: 27.10.2026. **P2 bleibt PARTIAL**, ebenso P3–P8.

## Umfang

Neues eigenständiges, opt-in `FrozenPressureFluidWrench3D::apply` in
`include/upc/pressure_wrench_reaction3d.hpp`. Die Methode verwendet
ein bereits projiziertes 3D-MAC-Druckfeld und die daraus ermittelte
statische Körperkraft/-drehmomentantwort als Eingabe. Anders als die
frühere reine lineare Reaktion wird nun auch das entgegengesetzte
**Drehimpulsäquivalent** auf interne, vollständig benetzte MAC-Faces
angewendet. Die Körpermaske und sämtliche Solid-Normal- und
Wasser-Luft-Grenzfaces bleiben unverändert.

Der Operator wählt nur Flüssig-Flüssig-Faces, deren mindestens eine
Nachbarzelle eine direkte Sechs-Nachbarschaft zum gewählten Körper hat.
Für jede ausgewählte Face-Geschwindigkeitskomponente gilt die diskrete
Masse `m_f = rho * dx^3`. Die sechs Impulsbedingungen lauten:

- `sum(m_f * delta_v * e_axis) = J_fluid`
- `sum((r_face - r_centre) x (m_f * delta_v * e_axis)) = L_fluid`

Die 6x6-Gram-Matrix `sum(c_i*c_i^T)` wird mit partieller
Pivotwahl gelöst. Zur Konditionierung wird Drehimpuls mit `dx`
skaliert. Das Ergebnis minimiert die Summe der quadrierten diskreten
Face-Impulse unter den sechs Nebenbedingungen. Bei unzureichendem
Rang, ungültigen Masken, NaN/Inf oder Overflow werden **vor**
jeglicher Face-Modifikation Ausnahmen ausgelöst. Null-Wrench bleibt
eine No-op. Es gibt keine heimliche Freigabe eines vollständigen
Fluid-Festkörper-Modells.

## Unabhängiger numerischer Test

`tests/p2_pressure_wrench_reaction3d_tests.cpp` konstruiert echte
3D-MAC-Druckprojektionen für `8x7x6`, `12x11x10` und
`16x15x14`, je mit offener Wasser-Luft- oder geschlossener
Festkörpergrenze und zwei Druck-Offsets. Ein vollständig benetzter
`2x2x2`-Körper erzeugt unter analytischem linearen Druckgradienten
`(7,-11,5) Pa/Zelle` die unabhängige Kraftreferenz
`(-0.5376,+0.8448,-0.3840) N`. Der um
`(+0.013,-0.017,+0.011) m` versetzte Drehpunkt erzwingt ein
**nichtverschwindendes** Druckdrehmoment, das unabhängig als
`(r_geometrisch-r_centre) x F` berechnet wird.

Der Test zählt selbständig alle berechtigten Faces, prüft alle anderen
Faces auf bitgenaue Unverändertheit und summiert die **tatsächlichen**
`rho*dx^3*(v_neu-v_alt)`-Impulse und ihre Kreuzprodukte für alle
sechs Freiheitsgrade. Er prüft außerdem analytische Kraft-/Drehmoment-
Referenzen, Energieproxy, Divergenz vor/nach einer **separaten**
Druckprojektion, Negativfälle und unveränderten Zustand nach
fehlgeschlagenen Eingaben. Das Python-Gate kontrolliert die native
CSV zusätzlich unabhängig von der C++-Assertion.

## Grenzen

Dies ist eine **diskrete Pre-Projection-Face-Impulsbilanz**, keine
physikalisch abgeleitete kontinuierliche Navier-Stokes-/FSI-Gleichung.
Die nachfolgende Druckprojektion kann den Impuls verändern; die
Face-Masse ist nur ein diskreter Proxy, nicht das volle
Finite-Volume-Momentum. Die Geometrie ist eingefroren, keine
dynamischen No-Slip-Körperwände, kein Body-Remeshing/VOF-Verdrängen,
kein vollständig gekoppelter Drehimpuls nach Projektion, keine
globale Fluid-/Körperenergie, keine externen 3D-Messreferenzen,
keine neue UE5.8-Native-E2E-Abnahme. Die bestehenden P2–P8
Vollphasen-Gates bleiben deshalb offen.

## Reproduktion und Nachweis

MSVC/Ninja Release im bestehenden `build`-Ordner.
`upc_p2_pressure_wrench_reaction_tests.exe` und vollständige
`ctest --output-on-failure`-Suite. Gate:
`scripts/p2_pressure_wrench_reaction_acceptance.py`.
Maschinenlesbar:
`ENGINE TEST STUBE/runs/p2_pressure_wrench_reaction_partial.json`.
Quell-, Binär- und Testlog-SHA256 sind an den jeweiligen Run gebunden.
Der Report darf nur `partial_accepted=true` und
`phase_complete=false` melden, wenn sämtliche Checks tatsächlich
bestanden sind.
