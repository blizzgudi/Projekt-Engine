# P2 Teilmeilenstein: Schwerkraft und hydrostatischer Druck in bewegtem Wasser/Luft-MAC/VOF (02.11.2026)

## Implementierter, begrenzter Umfang

Der opt-in-Pfad `p2_two_phase_mac_vof_step3d(state, max_pcg_iterations, gravity_m_s2)`
wendet eine konstante SI-Schwerebeschleunigung auf **innere** 3D-MAC-Faces an.
Die sechs geschlossenen Boxwände bleiben undurchlässig. Anschließend löst
der bereits vorhandene variable-Dichte-PCG den gemeinsamen Zweiphasendruck;
die korrigierten Face-Geschwindigkeiten transportieren die Wasser-VOF-Fraktion
und aktualisieren die Wasser-/Luft-Mischdichte. Ohne dritten Parameter ist
die Beschleunigung null; bestehende Aufrufer bleiben kompatibel.

Der zusätzliche Ledger prüft die diskrete, dichtegewichtete Face-Impulsquelle
und deren Arbeit gegen den Druckimpuls und die orthogonale Projektionsenergie.
**Dieser Ledger ist eine diskrete MAC-Face-Quadratur, keine globale
physikalische Wasser-/Luft-Energie- oder FSI-Bilanz.** Insbesondere fehlen
Potentialenergieänderung durch VOF-Remap, Impulsadvektion, bewegte Festkörper
und vollständige Grenzflächenspannungen. Die gesamte Operation bleibt
transaktional; ungültige Beschleunigung, Druckfehler oder VOF-CFL rollen
zurück.

## Native Reproduktion

Projektwurzel: `D:\Projekt Engine`. MSVC Release:

    cmake --build "Engine Core\UnifiedPhysicsCore\build" --config Release --parallel 4
    "Engine Core\UnifiedPhysicsCore\build\upc_p2_two_phase_gravity_tests.exe"
    ctest --test-dir "Engine Core\UnifiedPhysicsCore\build" -C Release --parallel 4 --output-on-failure
    py -3 "Engine Core\UnifiedPhysicsCore\scripts\p2_two_phase_gravity_acceptance.py"

Drei voneinander unabhängige hydrostatische Raster (4x4x4, 6x5x4,
8x6x5) prüfen über je vier Schritte die exakte diskrete
Schichtreferenz `p(z)-p(z-1)=0.5*(rho(z)+rho(z-1))*g*dx`.
Die Messfehler sind 9.09e-13, 6.01e-11 und 5.64e-11 Pa; maximale
Restgeschwindigkeit 3.79e-17, 1.93e-14 und 3.91e-14 m/s.
Zwei geneigte 3D-Wasser/Luft-Grenzen (6x5x4, 8x6x5) zeigen nach
je acht Schritten echten horizontalen und vertikalen Schwereantrieb:
u_max 0.30360/0.20497 m/s, w_max 0.18652/0.17559 m/s,
VOF-Aenderung 0.04290/0.04542; maximaler Massenfehler
1.28e-13/1.71e-13 kg. Diskrete Energie-Residuals
2.64e-16/6.52e-16 J. Der native Test meldet **1904 Assertions**,
die gesamte Windows-CTest-Suite **137/137 PASS**.

Ein echter negativer Vorab-Build wurde vor Implementierung des
neuen API-Pfades mit Exit 2 protokolliert; die C++-Fehler
`Funktion akzeptiert keine 3 Argumente` und fehlende Ledger-Felder
sind in `ENGINE TEST STUBE/runs/p2_two_phase_gravity_initial_negative.txt`
erhalten. Die Negativfälle verwerfen NaN-Schwerkraft und einen zu
großen geneigten Schwereantrieb ohne Zustandsmutation. Die unabhängige
Python-QA prüft die native Reproduktion, Messzeilen, manipulierte
Protokolle, vorhandene MARIN-Negativmessung, das offizielle Phase-Gate
und SHA-256-Artefakte.

## Abnahmegrenze

**P2 bleibt PARTIAL.** Dieser Nachweis ist keine experimentelle
Dreigitterkonvergenz. Das bestehende MARIN/SPHERIC-H1-H4-6s-Gate
ist negativ; P1-P8-Drucksensoren und gemessene unabhängige
Gitternachweise sind nicht abgenommen. Ebenso offen:
stresskonsistente bewegte Grenzfläche, vollständiger konservativer
Impulstransport, bidirektionale dynamische Fluid-Festkörper-Kopplung,
globale physikalische Masse/Impuls/Energie samt Potentialenergie
und Unreal-5.8-Native-E2E für diesen neuen Pfad.
