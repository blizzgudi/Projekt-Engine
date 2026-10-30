# P2-Teilabnahme: nichtlineares 3D-Druckmoment um beliebigen Referenzpunkt (30.10.2026)

## Implementierung und Umfang

Neuer isolierter C++17-Adapter `include/upc/p2_pressure_wrench_reference3d.hpp`
berechnet aus dem vorhandenen diskreten Cut-Cell-Drucklastoperator
`P2MovingBoxPressureTraction3D::audit` das Drehmoment um einen
frei gewaehlten, endlichen Referenzpunkt und integriert Kraft und Drehmoment
ueber einen Zeitschritt zu linearem und angularen Impuls.

Die Transformation verwendet exakt
`M_ref = M_boxcenter + (center_mid - reference) x F`.
Die entgegengesetzten Impulse werden **nur diagnostisch** erfasst.
Sie werden weder an einen MAC-Fluidimpuls noch an eine dynamische
rotierende Starrkoerper-Geometrie uebertragen.

## Unabhaengige quantitative Referenz

Ein nichtlineares dreidimensionales, zellzentriertes Druckfeld
`p(i,j,k) = 100 + 3 X - 2 Y + 4 Z + 0.25 (X-4.5)^3 (Y-4.5)` Pa
mit `X=i+0.5`, `Y=j+0.5`, `Z=k+0.5` erzeugt
ein echtes, nichtverschwindendes Drehmoment um die Boxmitte.
Die fraktionale Box liegt in Zelleinheiten von (3.2,3.1,3.3)
bis (5.7,5.6,5.8). Sie bewegt sich mit
`sign*(+0.11,-0.08,+0.09)*dx` in drei Achsen.
Der Referenzpunkt liegt bei (3.4,4.9,4.1)*dx.

Der native Test und das separate Python-Abnahmeskript pruefen
die 8^3- und 12^3-Gitter, beide Bewegungsrichtungen,
Kraft, intrinsisches Drehmoment, verschobenes Drehmoment,
exakte fraktionale Volumen-Druckarbeit, Gegenimpulsbuchung,
Druck-Gauge-Invarianz (+500 Pa), unveraenderte Eingaben,
ungültige Referenz und Zeit sowie die **Ablehnung**
einer dynamischen Translation bei nichtverschwindendem Drehmoment.
Die Python-Referenz integriert Druck pro benetzter Teilflaeche
und die Druckarbeit ueber die exakt ueberlappten Alt-/Neuvoxelvolumina.
Die numerischen Sollwerte werden unabhaengig vom C++-Ausgabewert
berechnet.

## Reproduktion

- `scripts/p2_pressure_wrench_reference_full_build.cmd`: MSVC-Release
  Standalone-Build, vollstaendige bestehende CTest-Regression.
- `scripts/p2_pressure_wrench_reference_acceptance.py`:
  separate analytische Geometrie-/Kraft-/Drehmomentreferenz, native
  Assertions, CTest-Protokoll und SHA-256-Quell-/Binarynachweise.
- `ENGINE TEST STUBE/runs/p2_pressure_wrench_reference_partial.json`
  und `p2_pressure_wrench_reference_full_ctest_20261030.txt`.

Die neue Probe wird absichtlich ohne Aenderung an historisch
fingerprintgeschuetzten CMake-/Headerdateien separat kompiliert.

## Nicht nachgewiesen

Das ist eine **P2-Teilabnahme**, keine P2-Vollabnahme:
keine orientierte/rotierende Boxkollision, keine dynamische
zweigerichtete Fluid-Festkoerper-Impulsrueckgabe, keine global
geschlossene physikalische Energie-/Drehimpulsbilanz,
keine extern gemessene 3D-CFD-Referenz, keine spannungskorrekte
Wasser-Luft-Grenze und kein neues UE5.8-Native-E2E.
P2 bis P8 bleiben INCOMPLETE.
