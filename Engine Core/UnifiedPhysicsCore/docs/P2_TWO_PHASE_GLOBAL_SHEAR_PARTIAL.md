# P2 partial: global implicit 3D water/air tangential shear (02.11.2026)

## Scope and motivation
The earlier fixed-grid two-phase shear operator applies a backward-Euler
water/air pair update in a deterministic *sequential* face order. At cells
touching multiple interfaces this is a Gauss-Seidel-like operator split:
results depend on face traversal and do not solve the simultaneous 3D
coupled shear equations. This milestone adds an opt-in **global** implicit
mass-weighted SPD finite-volume solve for all resolved 0/1 water/air
interfaces in a three-dimensional, cell-centered grid. The earlier operator
remains available and unchanged.

For each Cartesian velocity component k, only interface faces whose normal
axis is different from k carry tangential traction. The discrete equation is

    M_i (u_i^{n+1} - u_i^n)
      = sum_{j interface} dt * A/(dw/mu_w + da/mu_a) * (u_j^{n+1}-u_i^{n+1})
        + dt * A * (surface source allocated by interface resistance).

The interfacial Marangoni source is split with resistance weights
ra/(rw+ra) to water and rw/(rw+ra) to air, matching the existing two-cell
interface formulation. Its component normal to any active interface is
rejected. Jacobi-preconditioned conjugate gradients solves the SPD system
per component; a separately recomputed final residual must pass.
Invalid geometry, nonfinite input or nonconvergence rejects transactionally.

Mass is constant because phase geometry is frozen. Summed discrete impulse
balances the external surface source. The exact backward-Euler kinetic
identity is checked using interfacial viscous dissipation, implicit time
discretization dissipation, and resistance-weighted surface work.

## Reproduction
Run only under D:\Projekt Engine:

    Engine Core\UnifiedPhysicsCore\scripts\p2_two_phase_global_shear_build.cmd
    py -3 "Engine Core\UnifiedPhysicsCore\scripts\p2_two_phase_global_shear_acceptance.py"

MSVC/Ninja Release build, new native executable and all CTests are required.
The independent Python QA re-solves the 27-cell checkerboard's three
tangential linear systems by dense Gaussian elimination (not the C++ PCG),
compares nine sampled velocity components and the exact planar two-mass
Couette reference, rejects altered native traces, verifies a second native
run, the negative historical MARIN report, source/binary/log SHA-256, and
the official phase gate.

## Native result (02.11.2026)
- Global planar 4x4x4 Couette: 16 interface faces; exact two-mass
  implicit slip 0.56867137687403069 m/s; water y 0.00051697397897639138
  m/s; air y 0.56918835085300712 m/s.
- Global 3x3x3 checkerboard: 54 water/air interface faces, 18 per axis;
  maximum absolute impulse residual 3.3306690738754696e-16 N s;
  energy residual -2.2490524694614589e-17 J;
  PCG final residual <=5.5943312324395099e-17 kg m/s.
- 268 new native assertions PASS; complete Release CTest 134/134 PASS.
  Native negative inputs and a no-interface invariant were exercised.

## Negative evidence / limits
- Historical real MARIN/SPHERIC >=6s H1-H4 water-height RMSE remains
  above the 0.05 m threshold. This run deliberately does NOT rerun
  expensive full 6s simulations or invent new P1-P8 sensor convergence.
- The new operator still has a fixed sharp **cell-centered** phase mask:
  it does NOT solve the moving VOF/PLIC free surface, staggered MAC
  air flow, normal stress, pressure projection, capillary work or
  dynamic bidirectional fluid-solid coupling.
- No independent measured 3-grid flow convergence, global fully
  coupled FSI energy balance, or UE5.8 Native E2E for this operator.
- The complete P2 phase must remain PARTIAL; this is a narrow verified
  numerical subcomponent, NOT a full-phase acceptance.

Evidence: ENGINE TEST STUBE/runs/p2_two_phase_global_shear_partial.json,
including native/build/CTest/gate logs and SHA-256 fingerprints.

## QA-Failures reproduced and corrected
The first independent QA attempt failed because a second incremental Ninja
build printed only `ninja: no work to do` and therefore did not contain the
executable link name demanded by the build-log parser. The existing native
binary had already passed; forcing a *target-only* relink produced a new
verifiable MSVC linker log without rerunning the expensive MARIN case.
A second QA attempt correctly caught a weak negative fixture: prepending
`9` to a very small PCG residual still left it below the 1e-12 threshold.
The forged fixture was corrected to set the residual to 9; seven altered
traces are now rejected by the independent parser. Neither failed QA
attempt was counted as an acceptance.

## Erweiterung: ortsabhaengige Dichte und Viskositaet (02.11.2026)

Die optionale `P2GlobalShearCellProperties3D`-Eingabe liefert pro 3D-Zelle
separate Dichte (kg/m3) und dynamische Viskositaet (Pa s). Leere Arrays
erhalten den urspruenglichen konstanten Materialpfad. Jede Zellmasse wird
mit der lokalen Dichte berechnet; jeder Wasser/Luft-Grenzflaechenwiderstand
verwendet die beiden lokalen Halbzell-Widerstaende
`dx/(2*mu_water)+dx/(2*mu_air)`. Der globale SPD/PCG-Operator, die
Marangoni-Aufteilung und die Impuls-/Energiebilanz benutzen denselben
lokalen Widerstand. Ungueltige Groessen, NaN, negative Dichte und
nicht aufloesbare Viskositaet werden ohne Zustandsaenderung abgelehnt.

Echte MSVC-Release-Pruefung: 3x3x3 heterogenes 3D-Schachbrett,
54 Grenzflaechen, alle 27 Zellen mit unterschiedlichen Dichten und
Viskositaeten, unabhaengiger dichter 27x27-Gauss-Vergleich in C++ und
zusaetzliche Python-Referenz fuer ausgewaehlte Zellen. Maximaler
C++-Dense/PCG-Geschwindigkeitsfehler: 6.1388394367867249e-14 m/s;
Energierest -5.6733208015908969e-18 J. Native 450 Assertions PASS;
134/134 CTest PASS. Build-/CTest-/Native-/Gate-Logs und SHA256 im
aktualisierten Teilreport. Keine neuen MARIN-6s-Simulationen.

**Negativ:** Es gibt weiterhin nur eine Phase je scharfer Zellmaske,
keine gemischten VOF-Zellen, keine gekoppelte bewegte Luft-/MAC-Strömung,
keine dynamische FSI und keinen nachgewiesenen H1-H4/P1-P8-Mess-PASS.
Die historischen MARIN-Fehler bleiben unveraendert; P2 bleibt PARTIAL.

QA-Negativbefund: Die erste neue Falschdatenprobe ersetzte nur den
Praefix `dense_max_error=` durch `dense_max_error=9`; dadurch entstand
`9e-14`, das weiterhin innerhalb der erlaubten Toleranz lag. Der
unabhaengige Parser wies diesen zu schwachen Negativtest zu Recht als
QA-Fehler aus. Die Probe ersetzt jetzt den ganzen Messwert durch `9`.
Der urspruengliche QA-Fehllauf bleibt dokumentiert und ist kein PASS.


## Erweiterung: implizite tangentiale Scherung innerhalb beider Phasen (02.11.2026)

Der globale SPD/PCG-Operator besitzt nun den optionalen Schalter
include_bulk_tangential_shear=true. Er koppelt neben Wasser/Luft-Faces
auch benachbarte Zellen derselben Phase durch den lokalen harmonischen
viskosen Widerstand. Der Default false bleibt exakt der bisherige
reine Grenzflaechenoperator. Marangoni-Quellterme werden ausschliesslich
an echten Wasser/Luft-Faces verteilt. Die Energiebilanz trennt die
innerphasige Dissipation (bulk_viscous_dissipation_j) vom gesamten
viskosen Verlust; Impuls und Rueckwaerts-Euler-Dissipation bleiben bilanziert.

Echter nativer MSVC-Release-Nachweis: heterogenes 3x3x3-Gitter mit
9 Wasser/Luft-Faces und 45 innerphasigen Nachbar-Faces; unabhaengige
27x27-Dichtmatrixloesung fuer alle drei Geschwindigkeitskomponenten,
mit unabhaengiger Python-Neuberechnung an drei Zellen. Maximaler
native Dense/PCG-Fehler 1.1359871376903641e-12 m/s;
innerphasige Dissipation 3.5936867260319951e-05 J;
Impulsrest 2.2659933825164291e-13 N s;
Energierest -2.162241703823701e-14 J.
632 native Assertions und 134/134 CTest-Suites PASS.
Zusatzfaelle: reines Wasser ohne Grenzflaechen, Opt-in/Default-Vergleich,
ungueltige lokale Dichte mit transaktionaler Ablehnung.
Die unabhaengige Python-QA verwirft veraenderte Native-Traces,
prueft die historischen negativen MARIN-6s-Messwerte und hinterlegt
Source-/Binary-/Build-/CTest-/Native-/Phase-Gate-SHA256 im Teilreport
ENGINE TEST STUBE/runs/p2_two_phase_global_shear_partial.json.

Negativ: Der erste Native-Lauf entdeckte einen echten Zaehlfehler:
interface_faces zaehlte faelschlich alle Kanten einschliesslich
innerphasiger Kanten. Die CTest-Abnahme wurde nicht gestartet;
der Fehler wurde korrigiert und anschliessend nativ neu getestet.
Weiterhin **keine** volle viskose Spannungstensor-Diskretisierung
(keine Normalspannung / Kreuzableitungen), kein dynamisches MAC/VOF
Wasser-Luft-Interface, keine gekoppelte Druckprojektion und keine
vollstaendige FSI-Impuls-/Energiebilanz. Die unabhaengige reale
MARIN/SPHERIC-6s-Messvalidierung bleibt negativ.
**P2 FULL = OFFEN.**
