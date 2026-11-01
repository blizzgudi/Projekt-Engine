# P2 Teilmeilenstein: gesaettigte VOF-Zellen transportieren Wasser weiter

Stand: 01.11.2026. **P2 Vollphase OFFEN.** Der Vergleich gegen reale,
unabhaengige Messungen scheitert weiterhin, trotz nachweislicher Verbesserung.

## Ursache und begrenzte Korrektur

Der bestehende CoupledMacWater3D-Volumentransport blockiert vorab den
eingehenden Fluss einer vollstaendig gefuellten Zelle durch die
Kapazitaetsbedingung 1 - fill_receiver. Dies schliesst simultanen
Durchfluss aus, selbst wenn in derselben Zelle bereits Wasser austritt.
Ein ganz gefuellter Wasserblock bewegt sich deshalb deutlich zu langsam.

Neuer expliziter opt-in-Pfad:
- include/upc/p2_vof_throughflow3d.hpp
- CoupledMacWater3D::set_conservative_throughflow_transport(true)

Wasserfluesse werden nun auf allen inneren Zellflaechen gleichzeitig
als 3D-Donor-Cell-Fluss berechnet. Eine vollstaendig belegte Zelle kann
in einem Schritt Eingang und Ausgang besitzen. Ein iterativer
konservativer Flussbegrenzer ueberprueft den Wertebereich [0,1].
CFL-, Material- und Hindernispruefung bleiben aktiv, und der
Legacy-Modus bleibt unveraendert der Default.

Dies ist **keine geometrische PLIC-Advektion** und **keine**
spannungskorrekte Wasser-Luft-Navier-Stokes-Simulation.

## Analytische und native Nachweise

Native Quelle: tests/p2_vof_throughflow3d_tests.cpp.
Ein mit 0,2 m/s transportierter, anfaenglich 6 Zellen langer
3D-Wasserblock auf 16×8×8, dx=0,10m, dt=0,05s,
hat exakt 0,9 Fuellgrad am hinteren und 0,1 am neuen vorderen Rand.
Die dazwischenliegenden Zellen bleiben voll. Die numerische
Loesung stimmt mit der analytischen Uebersetzung ueberein;
384 innere Flux-Transfers und geschlossene Massenbilanz.

Echte MSVC-C++17-Regression prueft ferner NaN, CFL, Aussenflaechen,
Hindernis, vollstaendigen Tank, unveraenderte Eingaben und
CoupledMacWater3D live in beiden Modi (je 0,09 m³ erhalten).
CMake/CTest registriert das native Programm dauerhaft.

## Unabhaengiger externer Messvergleich: weiterhin FAIL

Quelle: https://www.spheric-sph.org/tests/test-02 .
Original-MARIN-Dammbruch, 7.395 Messzeilen, H1-H4 und P1-P8.
Auf 32×10×10 Zellen mit dx=0,10m wurden der Legacy-Modus
und die opt-in-Variante jeweils 200 reale Zeitschritte a 0,01s
mit unveraendertem Tankzustand ausgefuehrt. Das numerisch
erhaltene Gesamtvolumen betraegt rund 0,6644 m³.

| Hoehensensor | Legacy RMSE (m) | Opt-in RMSE (m) | Vorab-Limit (m) |
|---|---:|---:|---:|
| H1 | 0,090425 | 0,089234 | 0,050 |
| H2 | 0,100170 | 0,079381 | 0,050 |
| H3 | 0,107548 | 0,092056 | 0,050 |
| H4 | 0,206725 | 0,157912 | 0,050 |

**Alle vier Fehler sinken, aber alle vier ueberschreiten das
zuvor gesetzte 0,05-m-Limit.** Kein experimenteller P2-PASS.
Insbesondere wird der Reservoirwasserstand weiterhin zu langsam
abgesenkt.

Reproduzierbar: scripts/p2_spheric_marin_partial_acceptance.py,
scripts/p2_spheric_marin_throughflow_probe.py und
scripts/p2_vof_throughflow_marin_acceptance.py.
Die JSON-Protokolle und Native-/CTest-Logs befinden sich in
ENGINE TEST STUBE/runs. Teilabnahme nur bei ehrlichen Teilflags.

## P2 Vollphasen-Abnahme bleibt gesperrt

Noch fehlen die reale Validierung ueber mindestens 6 statt 2 Sekunden,
der experimentelle Vergleich aller acht Drucksonden,
mehrere verfeinerte Gitter, die spannungskonsistente freie
Oberflaeche und Luftphase, dynamische Zweiwege-FSI,
globale Impuls- und Energieerhaltung und eine neue
UE5.8-Native-E2E-Pruefung. P2 bleibt PARTIAL, ebenso P3-P8.
ALL_COMPLETE=False.
