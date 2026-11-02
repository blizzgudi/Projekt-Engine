# P2 Teilfortschritt: Höhenfunktions-Krümmung im bewegten 3D-MAC/VOF-Kapillar-PCG (02.11.2026)

**Status: opt-in geometrische/gekoppelte Native-Teilabnahme, P2 weiterhin PARTIAL.**

## Reproduzierter Fehler

Der bestehende diffuse CSF-Krümmungsoperator erzeugt am ruhenden
sphärischen Tropfen mit Gitterverfeinerung zunehmende parasitäre
Geschwindigkeiten (siehe `P2_CAPILLARY_LAPLACE_REFERENCE_PARTIAL.md`).
Der neue unabhängige 3D-Höhenfunktionsoperator aus dem vorherigen
Meilenstein war noch nicht an die tatsächlich bewegte
MAC/VOF-/Druckprojektionskette angeschlossen.

## Implementierung

- `include/upc/p2_vof_height_curvature_field3d.hpp`: einmaliger
  O(N)-Preflight für **streng einwertige** Wasser-unter-Luft-
  Graphen auf jeder der drei Achsen. Ein vollständig nasser
  unterer Rand, vollständig trockener oberer Rand, monotone
  Füllungsanteile und genau eine echte Schnittzelle pro Spalte
  sind erforderlich. Aus den 3D-VOF-Anteilen werden die
  Spaltenhöhen und die 3x3-Krümmung berechnet.
- Tangentiale Randkrümmungen werden vom nächstliegenden
  inneren 3x3-Stencil übernommen: **Randextrapolation, keine
  validierte physikalische Kontaktwinkel-Bedingung**.
- `include/upc/p2_two_phase_capillary_mac_vof3d.hpp`:
  zusätzlicher expliziter Parameter `height_normal_axis=-1`.
  Der bisherige diffuse Pfad bleibt der Standard. Für
  `0/1/2` nutzt die echte kapillare MAC-Face-Kraft die
  neue Höhenkrümmung und die vorhandene variable-Dichte-
  PCG-Projektion. Der opt-in-Modus wird auch bis zum
  bewegten konservativen Impuls-/Viskositäts-/VOF-Schritt
  durchgereicht. Ungeeignete Geometrien werden **abgewiesen**,
  nicht stillschweigend diffus weiterberechnet.
- Neue CTest-Suite
  `physics_p2_vof_height_capillary_live_partial`.
  Bestehende C++-Schnittstellen bleiben durch den
  Defaultparameter kompatibel.

## Native MSVC-Release-Messungen

- Geometrie: unabhängig hergestellte sphärische Graphfläche
  `h(u,v)=0.05+sqrt(1-u²-v²)`, **nicht** ein vollständiger
  geschlossener Kugeltropfen, nicht echte geometrische
  Volumenintegration. Drei Achsen, drei Gitter 12³/20³/40³.
  Mittlere Schnittkrümmung an der Referenzspalte:
  2.0050121703 / 2.0018016051 / 2.0004501010 1/m,
  Referenz 2/R = 2 1/m. Unabhängiger
  3x3-Referenzabgleich und Vergleich zum separaten
  Höhenfunktionssampler.
- Planare schiefe Wasser-Luft-Fläche: stationärer
  Kapillar-PCG-Schritt mit vernachlässigbarer Restströmung
  (< 1e-12 m/s im Native-Test).
- Statischer Graph mit echter MAC-Kapillarkraft und
  variabler-Dichte-PCG: auf 12³
  `4.8768188784e-9 m/s` maximale Restgeschwindigkeit
  statt `2.2925185664e-6 m/s` im bestehenden
  diffusen Pfad bei identischer Geometrie.
- **Negativer Refinement-Befund**: auf 8³/12³/16³
  steigt die maximale statische Restgeschwindigkeit
  `2.3730534372e-9 / 4.8768188784e-9 /
  6.6957778928e-9 m/s`. Es liegt **keine**
  nachgewiesene Konvergenz der parasitären Strömung vor.
  Die Krümmungsgenauigkeit allein ist dafür kein Beweis.
- Gekoppelter bewegter 8³-Wasser/Luft-VOF-/Impuls-/
  Volltensor-/Druck-PCG-Schritt: von diffusem Pfad
  unterscheidbare MAC-Geschwindigkeit
  `5.76730019197e-7 m/s`, Divergenz
  `1.32543715586e-16 s^-1`, beide Phasenmassen
  im getesteten Schritt erhalten.
- **72 native Assertions** inklusive fail-closed-
  Tests für Kugeltropfen-Überhänge, ungültige
  Achsen, inkonsistente Dichten und kapillare
  Zeitschrittgrenze. Erster MSVC-Build scheiterte
  wegen nicht geladener VC++-Umgebung
  (`algorithm` nicht gefunden); nach
  `vcvars64.bat` erfolgreich neu gebaut.
  Negative und positive Logs bleiben erhalten.
- Kompletter CMake-/CTest- und offizieller
  Gate-Lauf sowie unabhängige Python-QA:
  `scripts/p2_vof_height_capillary_live_acceptance.py`.
  Quell-/EXE-/Log-SHA256 und manipulierte
  QA-Negativprotokolle in
  `ENGINE TEST STUBE/runs/p2_height_capillary_live_partial.json`.

## Nicht nachgewiesen

Die neue Rekonstruktion ist nur für eine monotone
einwertige Grenzfläche geeignet. Geschlossene
Tropfen, Spritzer, Überhänge, freie Kontaktlinien,
beidseitig geschnittene Spalten und die
physikalisch korrekte PLIC-Grenzflächenspannung
sind **nicht** gelöst. Es fehlen weiterhin
die globale physikalische Oberflächen-/Impuls-/
Energie-Bilanz, dynamische bidirektionale
Festkörper-FSI, gemessene MARIN/SPHERIC
H1–H4/P1–P8-Konvergenz über mindestens 6 s
auf drei Gittern und der native UE5.8-E2E
für diesen Pfad. `FULL_SCOPE_OPEN["P2"]`
bleibt unverändert; **kein Full-PASS**.
