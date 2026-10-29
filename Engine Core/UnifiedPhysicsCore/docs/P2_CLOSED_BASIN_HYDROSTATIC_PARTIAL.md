# P2 Teilabnahme: hydrostatische Volumenverdrängung im geschlossenen 3D-Becken (29.10.2026)

**Status: PARTIAL. Keine vollständige P2- oder FSI-Abnahme.**

## Abgegrenzte Funktion

Der neue optionale C++-Operator
include/upc/p2_closed_basin_hydrostatic3d.hpp koppelt einen
vertikal bewegten achsenparallelen Quader an einen **geschlossenen,
quasistatischen** Wasserbehälter. Anders als beim vorherigen
Fixed-Level-Operator wird die freie Wasserhöhe nicht festgehalten:
Sie folgt exakt dem konstanten Wasservolumen und dem verdrängten
Quaderanteil. Für alte und neue Quaderpositionen werden 3D-Zellvolumina
berechnet und eine begrenzte Wasserfraktion bezogen auf das jeweilige
**offene Cut-Cell-Volumen** rekonstruiert. Das ist eine statische
Gleichgewichtsrekonstruktion, **keine** VOF-Advektion.

Seien A die horizontale Beckenfläche, B die Quadergrundfläche,
h seine Höhe, z die Quadermitte, V das Wasservolumen, rho die Dichte
und g die Schwerebeschleunigung. Es gilt A>B.

- Benetzte Quaderhöhe: w(z) = clamp((V-A*(z-h/2))/(A-B), 0, h).
- Wasseroberfläche: H(z) = (V+B*w(z))/A.
- Auftrieb: F_b(z) = rho*g*B*w(z).
- Wasservolumen: V = A*H-B*w, auch nach der Bewegung.
- Wasserpotential: U_w = rho*g/2 * (A*H² - B*(min(H,z+h/2)²-(z-h/2)²)), wobei der ausgeschlossene Term nur für H>z-h/2 gilt.
- Bewegung: dz = dt*(v0+v1)/2; m*(v1-v0) = dt*(mean(F_b)-m*g).
- Bilanz: delta(K_body + m*g*z + U_w) = 0 bis Rundung.

Der mittlere Auftrieb wird über die tatsächliche Verschiebung an
den beiden Regimegrenzen (voll/teilweise/trocken) stückweise exakt
integriert. Die implizite skalare Bewegungsgleichung wird mit
Intervallhalbierung gelöst. Die Quadergeometrie wird mit dem bestehenden
Cut-Cell-Operator gegen Domain- und 1-Zellen-CFL-Grenzen validiert.
Negative Volumina, Überfüllung, Rotationen, seitliche Bewegung und
inkonsistente Starrkörperzentren werden abgewiesen.

## Prüfung

Der neue native Test
tests/p2_closed_basin_hydrostatic3d_tests.cpp prüft zwei 3D-Raster
(8³/12³), sechs Anfangswasserstände je Raster, g=0/9.81 und zwei
vertikale Anfangsgeschwindigkeiten. Eine unabhängige numerische
Auftriebswegintegration mit 8192 Mittelpunkten und eine separat
implementierte Wasserschwerepotential-Formel prüfen Impuls und Energie.
Zusätzlich: tatsächliche 3D-Wasserfraktionsrekonstruktion, Massenerhalt,
Grenzen 0..1, neutraler Auftrieb, Zeitumkehr und Fehlerfälle.

Die reproduzierbaren Zahlen und die vollständige MSVC-CTest-Ausgabe
stehen nach erfolgreichem Lauf unter
ENGINE TEST STUBE/runs/p2_closed_basin_hydrostatic_partial.json
und p2_closed_basin_hydrostatic_full_ctest_20261029.txt.
scripts/p2_closed_basin_hydrostatic_acceptance.py prüft die
quantitativen Nachweise mit SHA-256 der Quellen.

## Physikalische Grenzen

Der Beckeninhalt wird in jedem Schritt **instantan ins hydrostatische
Gleichgewicht** gesetzt. Wasser besitzt keine aufgelöste kinetische
Energie, kein dynamisches Druckfeld und keine Wellenausbreitung.
Der entgegengesetzte Auftriebsimpuls wird **nur diagnostisch** ausgewiesen,
nicht auf MAC-Faces angewendet. Die geschlossene Energiebilanz ist die
eines idealisierten quasistatischen Körper-Wasser-Potentials und beweist
**keine** dynamische FSI-Impuls- oder Energieerhaltung. Die Zellfüllung
ist ein Gleichgewichtszustand, keine konservative zeitaufgelöste
MAC/VOF-Strömung. Es fehlen außerdem unabhängige gemessene 3D-CFD-
Referenzen, freie-Oberflächen-Spannungsbedingungen, Wasser/Boden- und
Thermomechanik-Gesamtkopplung sowie Unreal-5.8-Native-E2E dieses Pfades.
P2-P8 bleiben PARTIAL, bis die Vollkriterien unabhängig bestanden sind.
