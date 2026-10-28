# P2 – Variable konservative 3D-MAC-Impulsadvektion (Teilnachweis)

Stand: 28.10.2026. **P2 PARTIAL; P3–P8 bleiben ebenfalls PARTIAL.**

## Implementierung
- Neuer optionaler Header include/upc/p2_variable_mac_transport3d.hpp mit
  P2VariableMacTransport3D::step und self_advect. Keine Änderung am
  bisherigen CoupledMacWater3D oder an vorhandenen Operatoren.
- Für jede der drei MAC-Komponenten wird ein eigenes, periodisches,
  versetztes Dual-Kontrollvolumen benutzt. Die advectierende Geschwindigkeit
  auf dessen sechs Flächen stammt aus den benachbarten MAC-Faces:
  normaler Mittelwert aus zwei Normalfaces, transversaler Mittelwert aus
  zwei MAC-Faces beiderseits der versetzten Kontrollvolumengrenze.
- Expliziter Upwind-Fluss: q_new = q_old - dt/dx * sum(F_plus - F_minus),
  F = advector_face * upwind(q). Jede innere Flussfläche wird in zwei
  benachbarten Kontrollvolumina mit entgegengesetztem Vorzeichen bilanziert.
  Die redundant gespeicherte obere periodische MAC-Ebene wird exakt
  von der unteren gespiegelt und nicht doppelt integriert.
- Advektor muss diskret zellweise divergenzfrei sein; Form, NaN/Inf,
  periodische Seam und lokale ausgehende CFL <= 1 werden geprüft.
  Fehler werfen std::invalid_argument und verändern keine Eingabedaten.
- self_advect verwendet die drei Eingabekomponenten selbst als
  räumlich veränderlichen Advektor: **ein abgegrenzter nichtlinearer
  Konvektionsteiloperator**, noch keine Navier-Stokes-Zeitintegration.

## Reproduzierbare native Nachweise
- MSVC Release via scripts/p2_variable_mac_full_build.cmd;
  65/65 CTest-Suites; vollständiges Protokoll unter
  ENGINE TEST STUBE/runs/p2_variable_mac_full_ctest_20261028.txt.
- tests/p2_variable_mac_transport3d_tests.cpp:
  drei nichtkubische periodische 3D-Gitter (8×10×12, 12×14×16,
  16×18×20), positive/negative ortsvariable Scherströmung,
  vier zeitliche Upwind-Schritte und unabhängige Fourier-Eigenmode
  je Scherzeile. Für die gestaffelte V-Komponente wird die
  effektive Querflächengeschwindigkeit analytisch gemittelt.
- Unabhängige geschlossene Referenz:
  q_c(i,j,k,t) = mean_c + amplitude_c*(-1)^(i+j+k) *
  (1-2*dt*abs(a_eff(c,j)))^t, mit dt=0.6, dx=1;
  a_eff ist die y-abhängige U-Geschwindigkeit, für V der
  Mittelwert zweier benachbarter U-Zeilen. Die Gitter sind in
  allen Dimensionen gerade, damit die periodische Eigenmode passt.
- Zusätzlich nichttriviale einmalige Selbstadvektion eines
  3D-solenoidalen, quer veränderlichen U/V/W-Feldes: die drei
  Komponentenintegrale bleiben erhalten, die Werte ändern sich.
  Negativfälle prüfen Divergenz, lokale CFL, Periodizität, Größen,
  nichtfinite Eingaben und unveränderte Eingabedaten.
- scripts/p2_variable_mac_transport_acceptance.py prüft die
  nativen Ausgaben **unabhängig** gegen die analytischen Referenzen,
  65/65 CTest, Native-Assertion-Zahl und SHA-256-Hashes.
  JSON: ENGINE TEST STUBE/runs/p2_variable_mac_transport_partial.json.

## Strikte Grenzen
- Periodische Grenzen, einheitliches Gitter, konstante Dichte,
  keine VOF-Freifläche, keine viskosen oder bewegten Cut-Cell-Wände.
- Ein isolierter expliziter Advektionsschritt erzeugt im Allgemeinen
  **keinen divergenzfreien Nachfolgezustand**: Druckprojektion,
  Randbedingungen und Kopplung zum Live-MAC/VOF-Solver fehlen.
- Die drei MAC-Komponentensummen sind konservative diskrete
  Integrale, **keine validierte physikalische Gesamtimpuls-/Energiebilanz**.
  Upwind ist numerisch dissipativ und kein Beweis realer Turbulenz.
- Kein unabhängiger gemessener 3D-CFD-Referenzversuch, keine
  bidirektionale FSI und kein neuer UE5.8 Native-E2E für diesen Operator.
- Keine Vollphasenfreigabe: phase_complete=false, P2–P8 PARTIAL.
