# P2 Teilfortschritt: 3D-VOF-Höhenfunktions-Krümmung (02.11.2026)

**Status: geometrische Teilabnahme, P2-Vollabnahme ausdrücklich OFFEN.**

## Reproduzierter Fehler und eng abgegrenzte Implementierung

Die bestehende diffuse CSF-Krümmung des bewegten Zweiphasen-MAC/VOF-
Solvers zeigt bei ruhenden Kugeltropfen **nicht konvergierende**
parasitische Geschwindigkeiten: 5,9607e-7 / 1,0901e-6 /
3,0086e-6 m/s auf 8³/12³/16³ (siehe
`P2_CAPILLARY_LAPLACE_REFERENCE_PARTIAL.md`). Die separate
Young-Laplace-Referenz hat zuvor gezeigt, dass die variable-Dichte-
PCG-Projektion einen **exakt vorgegebenen** konstanten Drucksprung
balancieren kann. Die fehlende geometrisch konsistente
Krümmungsrekonstruktion ist damit ein konkreter Engpass.

Neues Modul:
`include/upc/p2_vof_height_curvature3d.hpp`.
Es berechnet für **lokal einwertige** Wasser/Luft-Freiflächen eine
3x3-Höhenfunktions-Krümmung direkt aus den vorhandenen 3D-VOF-
Wasseranteilen (ohne externe analytische Krümmung als Eingabe).
Alle drei Hauptachsen können als Wasserseite-unten/Luftseite-oben
gewählt werden. Für die beiden tangentialen Richtungen u,v gilt:

`h(u,v) = dx * sum_k alpha(u,v,k)`

`kappa = -[h_uu(1+h_v²)+h_vv(1+h_u²)-2 h_u h_v h_uv] /
                 (1+h_u²+h_v²)^(3/2)`

Die Auswärtsnormale ist proportional zu
`(-h_u,-h_v,1)`. Einheiten: Höhe in m, Krümmung in 1/m.
Es werden echte Volumenanteile summiert; die Methode
setzt einen zusammenhängenden, in Achsenrichtung monotonen
Wasserbereich voraus. Ungültige Wasseranteile, Überhänge,
zweite Grenzflächen, nicht aufgelöste Schnittzellen,
Randkontakt und ungültige Raster werden abgelehnt, nicht
stillschweigend in eine plausible Krümmung umgedeutet.

## Echte Native-Nachweise (MSVC Release)

Neuer Test: `tests/p2_vof_height_curvature3d_tests.cpp`,
CTest `physics_p2_vof_height_curvature_partial`.

Die 3D-Eingabefelder sind **hergestellte spaltenweise
Volumenanteile** für eine sphärische Höhenfläche
`h(u,v)=0.05+sqrt(1-u²-v²)` auf einer 1,2-m-Domäne.
Sie sind nicht als unabhängig gemessene oder exakt
flächenintegrierte gekrümmte VOF-Zellen auszugeben.
Analytische Kugelkrümmung: 2,0 1/m.

| Gitter | dx (m) | Berechnete Krümmung (1/m) | Absoluter Fehler (1/m) |
|---|---:|---:|---:|
| 12³ | 0,100 | 2,005012170259744 | 0,005012170259744 |
| 24³ | 0,050 | 2,001250776301850 | 0,001250776301850 |
| 48³ | 0,025 | 2,000312548751702 | 0,000312548751702 |

Alle drei Achsrichtungen wurden einzeln nativ geprüft;
der Fehler sinkt bei Gitterhalbierung um etwa Faktor 4.
Eine schräg liegende **ebene** Grenzfläche hat auf allen
drei Achsen Krümmung ungefähr -8,69e-14 1/m.
71 native Assertions PASS, inklusive negativer
Fehlereingaben und korrekter normierter Oberflächennormalen.
Die unabhängige QA verwirft **10/10 manipulierte Native-Protokolle**.
Der erste Build scheiterte nachweisbar an fehlender MSVC-
Umgebung und zwei Testquellfehlern; nach Aktivierung von
vcvars64 und Korrektur der C++-Testparameter kompiliert
und läuft die echte Native-Binärdatei. Beide Build-Protokolle
bleiben als positive und negative Artefakte erhalten.
Nach neuer CMake-Registrierung: **148/148 CTest PASS**.

Die unabhängige QA
`scripts/p2_vof_height_curvature_acceptance.py` liest die
echten Native-Ausgaben, führt die Binärdatei erneut aus,
vergleicht die drei Gitter mit einer eigenständig
berechneten diskreten analytischen Referenz und verwirft
manipulierte Protokolle. Quell-, EXE- und Log-Hashes
stehen im maschinenlesbaren Bericht unter
`ENGINE TEST STUBE/runs/p2_vof_height_curvature_partial.json`.

## Negative Resultate / nicht erfüllte Kriterien

1. Die gegenwärtige Referenzroutine prüft aus Sicherheitsgründen
   sämtliche VOF-Zellen pro Abfrage; für einen Live-Solver muss
   dies zu einem einmaligen Preflight und lokalem O(9*N)-Stencil
   umgebaut werden.
2. **Keine** Anbindung an den bewegten MAC/VOF-Kapillarstress oder
   die Druckprojektion. Der nicht konvergente Produktions-
   Tropfentest ist dadurch **nicht repariert**.
3. Die Höhe ist nur für lokal einwertige, monoton geschichtete
   Grenzflächen mit einem Hauptachsen-Darstellungsbereich
   gültig; Kugeltropfen mit Ober-/Unterseite, Überhänge,
   Spritzer und Wandkontakte erfordern andere Rekonstruktionen.
4. Die hergestellten Spaltenanteile sind kein experimenteller
   3D-CFD-Referenzfall und kein vollständiger geometrischer
   PLIC-Volumenintegrationsnachweis.
5. Die MARIN/SPHERIC-H1–H4/P1–P8-Messabnahme über >=6 s und
   drei unabhängige Gitter bleibt negativ/offen; die
   dynamische bidirektionale Fluid-Festkörper-Kopplung,
   globale Energie-/Impulsbilanz und neues UE5.8-Native-E2E
   sind weiterhin nicht nachgewiesen.

**Offizieller Gate-Status P2: PARTIAL.**
Die Liste `FULL_SCOPE_OPEN["P2"]` wurde nicht reduziert.
