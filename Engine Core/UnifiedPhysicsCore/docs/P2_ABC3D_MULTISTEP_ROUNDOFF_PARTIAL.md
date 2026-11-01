# P2 Teilnachweis: ABC3D Mehrschritt-PLIC und Endpunkt-Rundungsbereinigung

Stand: 2026-11-01. **Keine P2-Vollabnahme.**

## Reproduzierter Fehler

Der bereits dokumentierte 12³-ABC-Test scheiterte bei Schritt 2 von 5 mit
`Unresolved periodic PLIC interface`. Die Ursache wurde in Schritt 1
reproduziert: 19 Wasserzellen hatten exakt den Wert
`nextafter(1.0, 0.0)`, darunter vier Zellen mit exakt verschwindendem
zentralem PLIC-Gradienten. Beispielsweise war eine Zelle mit
`f=0.99999999999999989` von voll gefüllten Nachbarn umgeben. Ein
physikalischer Interface-Normalenvektor lässt sich daraus nicht
eindeutig ableiten. Der alte Kernel lehnt das weiterhin korrekt ab.

## Abgegrenzte Implementierung

`include/upc/p2_viscous_mac_plic_roundoff_sequence3d.hpp` stellt
einen **optionalen** Mehrschritt-Adapter bereit. Er setzt ausschließlich
Wasseranteile innerhalb von acht Maschinen-Epsilons der exakten
Grenzen 0 und 1 auf die jeweilige Grenze. Ungültige Füllgrade,
echte isolierte Teilfüllungen mit unbestimmbarer Normale und
andere Geometriefehler bleiben harte Fehler. Keine erfundene
Normalenrichtung, kein allgemeiner Massenkorrektur-Fallback.
Die Summe der **absoluten** Rundungsänderungen wird begrenzt
und separat in m³ bilanziert. Der ursprüngliche Solver und seine
bestehenden Nachweise bleiben unverändert.

## Native-Reproduktion und numerische Ergebnisse

`scripts/p2_abc3d_multistep_probe_build.cmd` baut den separaten
MSVC-C++17-Release-Test, führt ihn aus und prüft die bestehenden
CTest-Regressionen. `scripts/p2_abc3d_multistep_roundoff_acceptance.py`
prüft Logs, quantitative Schranken, Quell- und Binärhashes.

12³ periodisches ABC-Beltrami-Feld, drei nichtverschwindende
Geschwindigkeitskomponenten, `nu=0.003 m²/s`, `dt=0.02 s`,
fünf Schritte bis `t=0.10 s`, geometrischer PLIC-Wasser-Slab:

- Vor der Korrektur: zweiter Schritt reproduzierbar FAIL.
- Nach opt-in Bereinigung: fünf vollständige gekoppelte
  Viskositäts-, Impuls-, Projektions- und PLIC-Schritte PASS.
- Relative L2-Abweichung von exakter kontinuierlicher ABC-Lösung:
  `0.0062461362601023132` (kein Beweis höherer Konvergenzordnung).
- Numerisches Verhältnis der MAC-Geschwindigkeits-Quadratsummen:
  `0.96470053598201633`; analytisch viskos:
  `0.97659128563391073`. Zusätzliche numerische Dissipation.
- Anfangs- und Endwasservolumen ungefähr `0.25 m³`,
  Rundungsbereinigungen in 90 Zellbesuchen mit kumulierter
  absoluter Volumenänderung `5.6715279967534287e-18 m³`.
- Korrigiertes Gesamtmassenresiduum
  `-1.7216975279968369e-16 m³`; maximale projizierte
  Divergenz `1.154431368208253e-14 s^-1`.
- Native Negativtests: unzulässige Bruchteile, NaN, leere Eingabe,
  echte unbestimmte Teilzelle, ungültige Schrittzahl/Zeitschritt/
  Viskosität und erschöpftes PLIC-Budget.

## Grenzen

Dieser Adapter stabilisiert lediglich einen nachgewiesenen
Floating-Point-Endpunktfall zwischen **periodischen**
konstante-Dichte-MAC/PLIC-Schritten. Es ist weder eine generelle
geometrische Mehrphasen-Rekonstruktion noch eine Reparatur
anderer PLIC-Überlappungen. Es fehlen stresskonsistente freie
Oberflächen, bewegliche Zweiwege-FSI, externe gemessene 3D-CFD-
Vergleiche, globale physikalische Energie-/Impulserhaltung
und Unreal-5.8-Native-E2E für diesen neuen Pfad.

**P0/P1 PASS; P2–P8 INCOMPLETE; ALL_COMPLETE=False.**
