# P2 Teilabnahme: Rueckfuehrung projizierter 3D-MAC-Face-Geschwindigkeiten (28.10.2026)

Der optionale P2PrescribedBoxMacVof3D speichert nach einem erfolgreich
projizierten Cut-Cell/VOF-Schritt jetzt die korrigierten **offenen**
MAC-Face-Geschwindigkeiten (m/s). Diese werden aus den konservativen
zeitintegrierten Face-Volumina durch Division durch
dt * dx^2 * zeitgemittelte Apertur berechnet. Geschlossene Faces
haben den Wert null. Das ist wichtig: Die bisherige fuer den VOF-Transport
verwendete *aperturgewichtete* Geschwindigkeit darf nicht als
physikalischer MAC-Praediktor wiederverwendet werden, da sonst die
Apertur bei der naechsten Projektion ein zweites Mal wirkt.

Die neue Opt-in-Methode advance_reusing_projected_mac(next_box, dt)
verwendet die zuletzt korrigierten MAC-Faces als Praediktor fuer den
naechsten Cut-Cell-Projektionsschritt. Vor der Projektion werden Faces,
die in der neuen zeitgemittelten Geometrie geschlossen sind, auf null
gesetzt. Das ist ein bewusst einfacher kinematischer Remask-Schritt,
**keine** konservative Impulsadvektion oder korrekte dynamische
Rekonstruktion einer freien Wasseroberflaeche. Der erste Schritt
erfordert weiterhin advance() mit externem MAC-Praediktor. Die
Rueckfuehrung ist optional; der bestehende Aufruf bleibt kompatibel.

Bei Fehlschlag von Geometrie, Druckprojektion oder VOF bleiben die
gespeicherten MAC-Faces ebenso wie Wasser, Koerperlage, offene Volumina
und Schrittzaehler unveraendert. Auch der Fall ohne vorherigen
MAC-Zustand wird explizit abgewiesen.

Native Abnahme: tests/p2_projected_mac_feedback3d_tests.cpp.
Geprueft werden 8^3, 12^3, 16^3, alle 3 Bewegungsachsen, beide
Vorzeichen, Saettigungen 0,5 und 1,0 sowie drei Schritte pro Fall,
davon zwei mit Rueckfuehrung. Analytische Wasserreferenz:
S * (n^3 - 1.5^3) m^3, GCL/Projektionsresiduen und der
Apertur-zu-MAC-Rueckrechnungswert werden je Fall getestet. Ein
synthetischer nichtdivergenzfreier Druckanteil und ein nichtnulliger
Zirkulationsanteil sind im Startpraediktor enthalten. Negative
Tests pruefen den uninitialisierten Aufruf, ungueltige Zeitschritte,
fehlende PCG-Iterationen und zu grosse Koerperbewegung mit Rollback.

Vollstaendige CTest-Regression und maschinenlesbarer Report:
ENGINE TEST STUBE/runs/p2_projected_mac_feedback_full_ctest_20261028.txt,
ENGINE TEST STUBE/runs/p2_projected_mac_feedback_partial.json,
scripts/p2_projected_mac_feedback_acceptance.py.

**Keine P2-Vollabnahme.** Es fehlen ein physikalischer zeitaufgeloester
Momentum-Praediktor und dessen Kopplung an den allgemeinen
CoupledMacWater3D, freie Wasseroberflaechen, dynamische
Fluid-Festkoerper-Gegenimpulse, globale physikalische Energie- und
Impulsbilanz, unabhaengige 3D-CFD-Messreferenzen und ein neues
Unreal-5.8-Native-E2E fuer diese Teilkomponente. P2-P8 bleiben PARTIAL.
