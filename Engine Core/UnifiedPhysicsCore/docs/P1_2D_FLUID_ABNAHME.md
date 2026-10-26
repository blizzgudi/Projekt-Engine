# P1 – 2D-Wasserphysik: vollständige CPU-MVP-Abnahme

Stand 2026-10-26, Projekt D:\Projekt Engine.
P1 bezieht sich auf den **konservativen zweidimensionalen
Shallow-Water-Basissolver** (tiefengemittelte Wasserhöhe/Impuls). Dies ist
NICHT das 3D-Wasser aus P2 oder die gesamte Materialphysik der späteren Phasen.

## Physikalisches Modell

- 2D-Flachwassergleichungen auf einem flachen, horizontalen Boden,
  konstante Erdbeschleunigung 9,81 m/s².
- Unbekannte pro Zelle: Wassersäulenhöhe h [m], tiefengemittelter Impuls
  hu und hv [m²/s].
- Impulsgleichungen enthalten hydrostatischen Tiefendruck
  0,5*g*h². Bodendruck: rho*g*h, wobei rho aus zentraler Materialbibliothek
  kommt und unterhalb der Wasseroberfläche konstant angenommen wird.
- Explizites Finite-Volume-Verfahren mit konservativem HLL-Riemannfluss und
  hydrostatisch reflektierenden Rand-/Hindernisbedingungen (keine Lecks).
- Bei trocken/nass-Übergängen werden die Grenzen der Vakuum-Rarefaktionswelle
  mit u±2*sqrt(g*h) berücksichtigt; adaptive Teilzeitschritte nach
  Wellengeschwindigkeit mit niedrigem CFL-Faktor, Positivitätsprüfung.
- Der Solver ist erster Ordnung: diskrete Stossfronten verschmieren.
  Die Position einer trockenen Front ist abhängig vom für die Messung
  festgesetzten Tiefenschwellenwert. Deshalb werden ausdrücklich die
  Fronten bei 1e-6 m und 0,002 m ausgegeben, nicht nur eine geschönte Zahl.

## Unabhängige analytische Referenzen

### Ritter 1892, Dammbruch auf trockenem Boden

Anfangsdaten: hL=1 m, hR=0 m, u=0; Bruchposition x=6 m,
12-m-Referenzkanal, t=0,25 s. Kein Kontakt mit den Wänden bis zur Abnahme.

Die unabhängige analytische Ähnlichkeitslösung enthält eine
Rarefaktionswelle zwischen -sqrt(g*hL) und 2*sqrt(g*hL).
Tiefe und Impuls der numerischen Simulation werden an allen Zellzentren
gegen den geschlossenen Ausdruck verglichen.

Bei dx=0,1 / 0,05 / 0,025 m betrugen die mittleren absoluten
Tiefenfehler nach der HLL-Korrektur:
0,006068 / 0,004186 / 0,002760 m.
Impulsfehler: 0,015214 / 0,010118 / 0,007006 m²/s.
Wasserbilanzfehler durchgehend deutlich unter 1e-9 m³.
Frontfehler am niedrigen Tiefenschwellenwert 1e-6 m:
0,284 / 0,109 / 0,029 m; die Fehler nehmen beim Verfeinern ab.
Bei 0,002 m sind die Fehler dagegen 0,216 / 0,291 / 0,304 m:
Diese erheblichen Isolinienabweichungen bleiben dokumentiert.
Die niedrige Schwelle weist numerisch verdünnte Wasseranteile nach und
ersetzt keine subzelluläre hochpräzise Interface-Rekonstruktion.

### Stoker-Wet-Bed-Dammbruch als zweite Referenz

Anfangsdaten: hL=1 m, hR=0,2 m, beide in Ruhe, x=8 m,
16-m-Kanal, t=0,3 s.
Stoker-Zwischenhöhe und Stoßgeschwindigkeit werden unabhängig vom
Solver aus Riemanninvarianten der linken Rarefaktion und aus
Rankine-Hugoniot-Relation der rechten hydraulischen Stoßwelle berechnet:
h*=0,5078714345 m, u*=1,8000070397 m/s,
Schockausbreitung=2,9693308796 m/s.

Auflösungen dx=0,1 / 0,05 / 0,025 m:
mittlere Tiefenabweichung 0,006906 / 0,004174 / 0,002451 m;
mittlere Impulsabweichung 0,016427 / 0,009896 / 0,005862 m²/s.
Schockortfehler: 0,04080 / 0,01580 / 0,00330 m.
Alle Fehler sinken unter Gitterverfeinerung.

### Weitere unabhängige Invarianten

- Hydrostatischer Bodendruck p=rho*g*h als exakte SI-Formel,
  statischer See bleibt still.
- 90-Grad-Achsenvertauschung (x/y) liefert nach Integration numerisch
  äquivalente Höhe und transponierte Impulskomponenten.
- Undurchlässige innere Sperrwand mit einer Öffnung: Wasser darf nur durch
  den Durchlass strömen und die Gesamtwassermenge muss erhalten bleiben.
- CFL-Teilzeitschritte, lange Simulation, initiales räumliches Geschwindigkeitsfeld:
  kein negatives/NaN-Wasser und kein unerklärter Volumenverlust.
- Ungültige Maße, negative Höhe, unphysikalische Volumen-/Materialdaten
  werden abgewiesen.
- Legacy-Tests, alle anderen P0/P2–P8-Tests laufen als Regression mit.

## Reproduzierbare Ausführung und Acceptance

Quellcode:
- include/upc/core.hpp
- tests/core_tests.cpp
- tests/ritter_reference_tests.cpp
- tests/stoker_wet_dam_tests.cpp
- tests/shallow2d_extended_tests.cpp
- benchmarks/p1_front_probe.cpp
- scripts/p1_acceptance.py

Native MSVC-Release-Build, kompletter CTest; dann:
  C:\Python314\python.exe scripts\p1_acceptance.py
und
  C:\Python314\python.exe scripts\phase_gate.py

Report: ENGINE TEST STUBE/runs/p1_acceptance.json
mit CPU-Messwerten, den zwei Referenzen, allen Abnahmekriterien und
SHA256-Quellfingerprints. P1 ist nur PASS, wenn kein relevantes
Prüfergebnis FAIL und sämtliche Nachweise mit aktuellem Code übereinstimmen.

## Grenzen

- Keine echte Viskosität, keine Oberflächenspannung, turbulente Wirbel
  oder Materialmischung, kein Bodenrelief, keine beweglichen Festkörper.
- HLL erster Ordnung ist numerisch diffus; die starke 0,002-m-
  Isolinienabweichung an der trockenen Front bleibt eine explizite Grenze.
- Druck ist hydrostatische **Boden**-Druckformel, kein 3D-Druckfeld.
- Dammbruchreferenzen sind ideal: trockener/nasser horizontaler,
  reibungsfreier Kanal; keine experimentelle Kalibrierung an Realflüssen.
- P2–P8 mit 3D-Fluid, Fluid/Solid-Kopplung, Wärme, GPU/Unreal bleiben
  davon unabhängig.

## Referenzen für unabhängige Theorie

- Ritter, 1892 (Trockenbett-Dammbruch), Pritchard & Hogg, Journal of Fluid
  Mechanics 473 (2002), pp. 265–274,
  https://people.maths.bris.ac.uk/~maajh/PDFPapers/JFMDambreak.pdf
- Stoker, Water Waves (1957); wet-bed-Fall siehe
  https://www.mdpi.com/2073-4441/15/21/3841
- Wellengeschwindigkeit und HLL Finite-Volume, Standard-Flachwasser-
  Erhaltungsgleichungen. Keine Übertragung auf vollständig dreidimensionale
  Navier-Stokes-Physik behauptet.
