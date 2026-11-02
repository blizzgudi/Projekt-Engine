# P2 Teilnachweis: echter 3D-Wasser/Luft-Impuls und volles symmetrisches Viskositätstensorfeld

Stand 02.11.2026. **Das ist kein vollständiges P2-PASS.**

## Neue tatsächliche 3D-Live-Kopplung

Eigener opt-in-Pfad:
include/upc/p2_two_phase_momentum_viscous_vof3d.hpp.

Der Pfad kombiniert **in einem einzigen echten gekoppelten Zeitschritt**:

1. Variabel-dichte Wasser-/Luft-MAC-PCG-Projektion mit 3D-Schwerkraft.
2. Konservative VOF-Transportflüsse in allen drei Raumrichtungen.
3. Zellzentrierte massenkonsistente 3D-Impulsadvektion mit denselben VOF-Massenflüssen.
4. Dichtegewichtete Rückübertragung auf staggered MAC-Faces mit separatem Rand-Halbzell-Ledger und zweiter MAC-PCG-Projektion.
5. Volles symmetrisches zellweise gemischtes Viskositätstensorfeld mit drei normalen und drei Scherkomponenten: XX, YY, ZZ, XY, XZ, YZ.
6. Dritte echte Variabeldichte-MAC-PCG-Projektion, nachdem die Viskosität die Gesichts-Geschwindigkeiten verändert hat.

Die Live-Masse wird unabhängig über Wasser- und Luftvolumen nach jedem Schritt überprüft. Die Viskositäts-Impulsänderung muss exakt der **ausgerechneten Viskositäts-Wandnormaltraktion und zusätzlichen Druck-Wandreaktion** entsprechen. Die Kinetikdifferenz von Viskosität und dritter Projektion muss exakt der diskreten Scherstress-Dissipation minus expliziter Zeitschrittkorrektur und Druckprojektionsdissipation entsprechen.

Alle Fehler bleiben transaktional; ein fehlerhafter später Viskositätswert oder PCG-Schritt übernimmt **keine** früheren Halbresultate in den Ursprungszustand. Der alte API-/Standardpfad wird nicht verändert.

## Echte überprüfte Native-Nachweise

Tests:
tests/p2_two_phase_momentum_viscous_vof3d_tests.cpp.

Auf 4x4x4 / 6x5x4 / 8x6x5 werden jeweils sechs echte Live-Updates mit Wasser/Luft-Mischungsdichte, Schwerkraft, Zell-Impuls-VOF-Advektion, dichtegewichtetem Face-Remap, sechs Tensor-Stresskomponenten und drei echten Druckprojektionen durchgeführt.

**5.459 Native-Assertions PASS**. Die Viskosität wird im Live-Step wirklich wirksam. Verglichen mit demselben laufenden Ausgangsfall ohne Viskosität unterscheiden sich MAC-Face-Geschwindigkeiten auf den drei Gittern um:

| Gitter | echter Viskositätseffekt | Scherstressarbeit | Max. Wandimpuls-Rest | Max. Energiesubschritt-Rest |
|---|---:|---:|---:|---:|
| 4x4x4 | 1,270e-5 m/s | 1,349e-6 J | 1,12e-16 kg m/s | 5,31e-18 J |
| 6x5x4 | 1,731e-5 m/s | 2,407e-6 J | 2,96e-16 kg m/s | 1,14e-17 J |
| 8x6x5 | 1,782e-5 m/s | 3,190e-6 J | 9,83e-16 kg m/s | 6,36e-17 J |

Die angesetzten erhöhten Test-Viskositäten dienen der unabhängigen Verifikation eines messbaren Effekts. Sie sind keine Materialparameterkalibration für MARIN/SPHERIC.

Vollständiger MSVC-CMake Release: **144/144 CTest-Suites PASS**, einschließlich aller vorigen P2- und P1-nativen Tests. Weitere QA durch
scripts/p2_two_phase_full_tensor_momentum_acceptance.py
mit expliziter Trennung zwischen numerischem Teil-PASS und physikalischer Vollabnahme. Logs, Binaries, SHA256 im bestehenden Unterordner ENGINE TEST STUBE/runs.

## Noch offene vollständige P2-Akzeptanzbedingungen

- **Keine scharfe, spannungskonsistente Wasser/Luft-PLIC-Grenzfläche.** Zell-Mischungsviskosität und freie Randbehandlung sind nicht die vollständigen Navier-Stokes-Interface-Sprungbedingungen.
- **Keine unabhängige experimentelle MARIN/SPHERIC-Messabnahme** H1-H4 und P1-P8 über das gesamte mindestens sechssekündige Fenster, mit mindestens drei räumlichen Gittern und vorab festen Toleranzen.
- **Keine dynamische Fluid-/Festkörper-Zweiwege-FSI**, vollständig mit bewegten/deformierten Randflächen, Druck-/Scher-/Wandimpuls und Festkörper-Energie.
- **Keine globale mechanische Navier-Stokes/VOF/FSI-Energieabnahme** und nicht alle MAC-/Halbzell-Randkräfte in einer physikalischen Gesamtbilanz.
- **Kein neues UE5.8-Native-E2E** für genau diesen erweiterten Zweiphasenpfad.

Die hier bestätigten Wandintegrale und Energieidentitäten gelten nur für den **fixen Nach-VOF-Viskositäts- und Drucksubschritt**. Sie beweisen keinen Gesamtimpuls- oder Gesamtsystemenergieabschluss.

**Phase P2 = INCOMPLETE; ALL_COMPLETE=False.**
