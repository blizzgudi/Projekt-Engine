# P2 Teilnachweis: transaktionale 3D-MAC-Druckprojektion mit Jacobi-PCG (01.11.2026)

**Status: PARTIAL, keine P2-Vollabnahme.** Eigenständiges Projekt: `D:\Projekt Engine`.
Die nachstehenden Werte stammen aus nativen MSVC-Win64-Release-Läufen und dem
maschinenlesbaren Bericht `ENGINE TEST STUBE/runs/p2_pcg_partial_acceptance.json`.
Der Bericht enthält SHA-256 für getestete Quellen, Binaries und das vollständige CTest-Log.

## Reproduzierter technischer Mangel und implementierte Verbesserung

Die bisherige 3D-MAC-Projektion verwendet einen sequentiellen
Gauss-Seidel-Drucklöser mit fester maximaler Iterationszahl. Ein kleiner
Druck-Update-Unterschied allein ist kein belastbarer Nachweis für
Divergenzfreiheit. Für eine räumlich aufgelöste 3D-Messvalidierung ist
ein numerisch kontrollierter, überprüfbarer Drucklöser erforderlich.

Implementiert wurde ein **explizit zuschaltbarer, transaktionaler
Jacobi-vorkonditionierter Conjugate-Gradient-Löser** auf dem bestehenden
MAC-Stencil: feste und äußere Wände erzwingen Null-Normalgeschwindigkeit
(Neumann), Wasser/Luft-Flächen erhalten den bisherigen Druck-Nullwert
mit halber Zellabstands-Korrektur (Ghost-Faktor 2). Für geschlossene
Neumann-Komponenten werden separate Druckeichungen mit Mittelwert null
und die algebraische Kompatibilität geprüft. Die reale
Druckgleichungs-Restgröße wird nach Abschluss **neu** aus `A*p-b`
berechnet. Anschließend wird die Divergenz sämtlicher nasser Zellen
einschließlich NaN/Inf kontrolliert. Bei Nichterreichen des Grenzwerts
wird der vollständige MAC-Zustand verworfen.

Die bestehende `project()`-Methode und der bisherige Live-Standardpfad
bleiben unverändert. `CoupledMacWater3D` erlaubt jetzt
`set_pcg_pressure_projection(true)` mit
`step_verified(...)` oder `step_verified_reprojected(...)`.
Der gekoppelte Aufruf führt adaptive Unterzeitschritte, VOF-Transport
und bei der zweiten Variante eine erneute Projektion der neuen Nassmaske
aus. Fehler führen zum atomaren Rollback ohne Transport-Callbacks.
PCG-Iterationszahl und maximaler Gleichungsrest sind auslesbar.

## Positive Native-Belege

| Test | Beobachtung |
|---|---|
| Hydrostatik 3D 8³, 12³, 16³ | Analytische Zellmittelpunkt-Drücke innerhalb 1e-4 Pa; PCG 29/48/67 Iterationen |
| 10³ mit komplexer Wasser-/Luft-/Festkörpermaske | PCG 79 Iterationen; Nasszell-Divergenz 2,608e-10 s⁻¹; Druck-RMS-Abweichung gegen 7.500 Gauss-Seidel-Sweeps 3,346e-7 Pa |
| Gekoppelter 3D-Teich | Masse 384 kg, Divergenz 5,637e-14 s⁻¹, 41 PCG-Iterationen |
| Gekoppelter 3D-Dammbruch, 3 Schritte | Masse 90 kg, Volumenstrom über den anfänglichen Damm hinaus (Füllgrad-Summe 0,778613), Divergenz 1,612e-10 s⁻¹ |
| Native Testprogramme | 621 + 1.462 Assertions PASS |
| Gesamter MSVC-Release-/CTest-Lauf | **117/117 PASS**, 0 FAIL |

## Negative QA – nicht unterschlagen

- Bewusst zu wenige PCG-Iterationen führen zur Ablehnung statt einem scheinbaren PASS.
- Ungültige Schrittweite, Iterationszahl und Divergenztoleranz werden abgelehnt.
- Ein extrem kleines `dt=1e-300` verursacht eine nichtendliche Druck-RHS und
  wird abgelehnt; nichtendliche Geschwindigkeiten/Divergenzen werden vor
  jeder Zustandsübernahme ausdrücklich geprüft.
- Ein gescheiterter gekoppelter Schritt verändert weder Wassermasse,
  VOF-Zustand, Druck noch Diagnosezähler; **0 externe Transport-Callbacks**.
- Die gemessenen MARIN-Druckfehler wurden in diesem Teilnachweis
  **nicht erneut gerechnet und nicht als verbessert behauptet**.
  Historische Zweigitter-Belege zeigen weiterhin experimentelle
  Nichtkonvergenz aller acht P1–P8-Druckkanäle und H1–H4-RMSE über 0,05 m.

## Reproduzierbarkeit und Grenzen

`python scripts/p2_pcg_partial_acceptance.py` prüft die nativen
Release-Binaries, den vollständigen CTest-Lauf, negative Marker und
SHA-256; Ergebnis in `ENGINE TEST STUBE/runs/p2_pcg_partial_acceptance.json`,
Log in `ENGINE TEST STUBE/runs/p2_pcg_ctest_full.log`.

Die exakten SHA-256-Werte der beiden Native-Testprogramme, der
sechs Quell-/Build-Dateien und des vollständigen CTest-Logs werden
reproduzierbar in `p2_pcg_partial_acceptance.json` ermittelt.
Nach einem Rebuild gelten ältere Binär-Hashes ausdrücklich nicht weiter.

**Nicht nachgewiesen:** spannungskonsistente Wasser-Luft-Grenze,
Wandnormalstress und geometrisch aufgelöste MARIN-Sensorflächen,
6-s-Referenzmessfenster, Gitterkonvergenz der H1–H4/P1–P8-Laborwerte,
bidirektionale dynamische Fluid-Festkörper-Kopplung und globaler
physikalischer Energie-/Impulshaushalt. Die Ghost-Fluid-Druckannahme
ist ausdrücklich kein Zwei-Phasen-Stressmodell.

**Nächste technische Arbeit:** PCG als separat auswählbare Druckprojektion
im echten MARIN-3D-Referenzlauf gegen die unveränderten Originalmessdaten
auf mindestens zwei Gittern evaluieren; negative oder nicht konvergente
Resultate protokollieren, danach Grenzspannungen und geometrische
Sensorflächen physikalisch konsistent verbessern. P2 bleibt PARTIAL.
