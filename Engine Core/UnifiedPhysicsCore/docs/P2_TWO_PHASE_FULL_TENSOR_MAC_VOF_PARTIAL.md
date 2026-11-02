# P2: vollständiger symmetrischer 3D-MAC-Viskositätstensor mit bewegter Wasser/Luft-VOF

Stand: 02.11.2026. **NUMERISCHER TEILFORTSCHRITT, KEINE P2-VOLLABNAHME.**

## Implementierte Physik

Neuer isolierter und geprüfter 3D-Finite-Volume-Operator:
include/upc/p2_two_phase_full_tensor_viscous_mac3d.hpp.

Er berechnet auf dem realen staggered-MAC-Gitter alle sechs
unabhängigen Komponenten des symmetrischen Newtonschen Tensors
tau = mu * (grad(v) + grad(v)^T). Drei Normalspannungen werden
an Zellzentren und drei tangentiale Scherspannungen auf den
passenden MAC-Kanten berechnet. Alle drei Geschwindigkeitsrichtungen
erhalten daraus gemeinsame diskrete Spannungskräfte.

Äußere Wände sind normal undurchlässig und tangential gleitend
(freie Schertraktion). Die viskose Energieabnahme und die
Normal-Wand-Impulsreaktion werden ausdrücklich gerechnet. Die
temporale Euler-Korrektur zweiter Ordnung wird nicht verschwiegen.

Wasser- und Luftdichten sind echte variable Zellwerte;
die partiellen Zellen verwenden jedoch eine interpolierte
Viskosität und harmonische Kantenkoeffizienten.
**Das ist KEINE aufgelöste bewegte Grenzflächen-Spannung.**
Young-Laplace, Grenzflächennormalen, Marangoni, dynamische
Festkörperreibung und echter kontaktphysikalischer FSI
werden dadurch nicht automatisch validiert.

Neuer opt-in Live-Zusammenbau:
include/upc/p2_two_phase_full_tensor_mac_vof3d.hpp.

Er führt den bereits vorhandenen bewegten variabel-dichten
Zweiphasen-MAC/VOF-PCG-/Gravitationsschritt aus, danach
den neuen 3D-Tensorviskositätsoperator auf dem aktualisierten
Wasser/Luft-Feld, und zuletzt eine echte variable-Dichte-
PCG-Reprojektion. Alle Schritte laufen auf einem
transaktionalen Kopiezustand. Bei fehlerhafter Druck-/Energie-
/Impulskontrolle wird der bisherige Live-Zustand nicht überschrieben.

## Tatsächliche native Abnahmen

Tests: tests/p2_two_phase_full_tensor_viscous_mac3d_tests.cpp
und tests/p2_two_phase_full_tensor_mac_vof3d_tests.cpp.

- Drei analytische diskrete 3D-Fourier-Moden auf 4x4x4, 6x5x4 und
  8x6x5, inklusive unabhängiger 2*d²u/dx²+d²u/dy²-Referenz.
  Maximale Abweichung unter 7e-18 m/s.
- Drei heterogene Wasser/Luft-Tests mit jeweils neun
  Druck-unabhängigen Spannungs-Zeitschritten. In allen drei
  Richtungen schließen normale Wandkräfte gegen Impulsänderung;
  Energie nimmt mit viskoser Dissipation ab.
  Maximaler Energierest unter 4e-16 J,
  maximaler Impulsrest unter 2e-15 kg m/s.
- Vier real gekoppelte 3D-MAC/VOF-Wasser/Luft-Szenen, je acht
  Schritte mit Schwerkraft, bewegt-obliquen Wasseranteilen,
  vollständigem symmetrischem Viskositätstensor und erneutem PCG.
  Der neue Tensor verändert nachweisbar die tatsächlichen
  3D-Face-Geschwindigkeiten; finale Divergenz unter 1e-15 s^-1.
- In bewegten Fällen maximale viskose Geschwindigkeitsänderung
  rund 1,52e-5 m/s. Der isolierte Viskositäts-/Druck-Energierest
  beträgt maximal 3,71e-16 J und Wand-Impulsrest
  maximal 3,36e-15 kg m/s. Maximaler Wasser-Masserest
  unter 2e-13 kg.
- Negative MSVC-Proben: NaN, nicht-physikalische Viskosität,
  undichte Wände, zu große Zeitschritte und PCG-Unteriteration
  werden fail-closed zurückgewiesen.
- **574 native Tensor-Assertions + 4.844 echte Live-Assertions PASS.
  Vollständiger MSVC/CTest: 143/143 Suites PASS.**

Rohbelege und die unabhängig prüfbaren Quell-/Binär-SHA256:
ENGINE TEST STUBE/runs/p2_two_phase_full_tensor_*.txt und
p2_two_phase_full_tensor_partial.json.

Reproduktion:
scripts/p2_two_phase_full_tensor_build.cmd und
scripts/p2_two_phase_full_tensor_acceptance.py.

## Fehlende gesamte P2-Abnahme

Diese Arbeit schließt nur den isolierten realen symmetrischen
Spannungs-/PCG-Teilalgorithmus auf einem bewegten Wasser/Luft-VOF.
Der zeitgleiche zellzentrierte/MAC-Impulstransport muss noch mit
stresskonsistenten scharfen Schnittflächen kombiniert werden.
Die **gesamte** mechanische Impuls-/Energiebilanz unter
bewegter Festkörper-FSI bleibt offen. Die unabhängige
SPHERIC/MARIN-H1-H4-/P1-P8-Messkonvergenz über sechs Sekunden
und drei echte Gitter sowie ein UE5.8-Native-E2E dieses
neuen Gesamtpfads sind nicht PASS.

**P2 = INCOMPLETE. ALL_COMPLETE=False.**
