# P2-Teilnachweis: impliziter 3D-Wasser/Luft-Scherimpulsaustausch (02.11.2026)

## Umfang und physikalische Diskretisierung

Neuer **isolierter**, C++17/MSVC-nativer Baustein include/upc/p2_two_phase_shear_exchange3d.hpp fuer genau eine vorgegebene planare Grenzflaeche zwischen zwei endlichen Wasser-/Luft-Kontrollvolumina. Die drei Geschwindigkeitskomponenten, Einheitsnormale, getrennte Massen, dynamische Viskositaeten, Zellmittenabstaende, Grenzflaechenflaeche, Zeitschritt und tangentialer Marangoni-Gradient sind explizite Eingaben. Dies ist ein realer *zeitlicher* Impuls-Update beider Phasen, nicht bloss eine Stress-Diagnose. Es ist **noch keine** Integration in CoupledMacWater3D und kein vollstaendiger 3D-Zweiphasen-CFD-Solver.

Fuer jede tangentiale Komponente gelten die beiden einseitigen viskosen Traktionen

tau_w = mu_w (u_interface - u_w)/d_w; tau_a = mu_a (u_a - u_interface)/d_a; tau_w - tau_a = grad_s(sigma).

Mit r_w=d_w/mu_w, r_a=d_a/mu_a, k=A/(r_w+r_a), b=dt*k, D=u_a-u_w wird der relative Geschwindigkeitsunterschied **implizit** aktualisiert:

D_new = [D_old + b*(r_w/m_a-r_a/m_w)*grad_s(sigma)] / [1+b*(1/m_w+1/m_a)].

Die Impulse sind I_w=b*(D_new+r_a*grad_s(sigma)) und I_a=-b*(D_new-r_w*grad_s(sigma)). Dadurch ist I_w+I_a=dt*A*grad_s(sigma) explizit bilanziert; bei Nullgradient sind die Phasenimpulse exakt gegenlaeufig. Die Normalgeschwindigkeit bleibt unveraendert. Einheiten: Pa*s, kg, m, m², s, N*s, J.

Der implizite Energiesatz lautet:

K_after-K_before = W_surface - dt*k*|D_new|² - (|I_w|²/(2*m_w)+|I_a|²/(2*m_a)).

Der letzte Term ist die zusaetzliche **numerische Backward-Euler-Dissipation**, keine erfundene physikalische Waerme. Das Oberflaechenarbeitskonto nutzt die durch die beiden Widerstaende gewichtete neue Tangentialgeschwindigkeit. Bei grad_s(sigma)=0 sinkt die kinetische Energie. Normalimpuls, Oberflaechenenergie, Druck, Kapillarwellen und Wasser/Luft-Massenadvektion sind hier **nicht** gekoppelt.

## Reale native Reproduktion

scripts/p2_two_phase_shear_exchange_build.cmd ruft MSVC-x64, CMake Release, das echte native Test-Executable und die gesamte CTest-Suite auf. Der separate QA-Lauf scripts/p2_two_phase_shear_exchange_acceptance.py prueft das native Log mit einer unabhaengig nachgerechneten Zwei-Massen-Referenz, fuehrt die Binary erneut aus, prueft negative Trace-Manipulationen, den negativen historischen 6-s-MARIN-Bericht und das offizielle phase_gate.py. SHA256 von Quellen, Binary und Build-/Test-/Gate-Logs stehen im JSON-Teilreport.

- Analytischer Couette-Fall: m_w=2 kg, m_a=1 kg, mu_w=2 Pa*s, mu_a=0.5 Pa*s, d_w=0.04 m, d_a=0.06 m, A=0.01 m², dt=0.2 s, D_old=1 m/s. Erwartet D_new=0.979020979020979, I_w=0.013986013986014 N*s, I_a=-I_w.
- Nichtnulliger Oberflaechengradient (0,3.295,-1.14) Pa und gedrehte 3D-Normale: Tangentialspannungssprung und Gesamtoberflaechenimpuls, konstante Normalgeschwindigkeit und Energiesatz werden geprueft.
- Verschiedene Zeitschritte bis 10000 s pruefen die implizite Stabilitaet; neun ungueltige Eingaben (u.a. Nullmasse, Nullviskositaet, NaN/Inf, falsche Normale, normaler statt tangentialer Oberflaechengradient) werden abgewiesen.
- **Dokumentierter Fehlversuch:** Der erste Native-Lauf endete mit Exit 23 (FAIL: Invalid patch accepted), weil der Test einen sehr grossen, aber endlichen Zeitschritt faelschlich als ungueltig ansah. Nur diese fehlerhafte Negativ-Testannahme wurde korrigiert (Infinity statt endlichem double::max); der Algorithmus blieb dabei unveraendert. Anschliessend Native- und Full-CTest-PASS. Diese negative Beobachtung ist nicht als physikalischer Solverfehler zu werten.

## Grenzen / offene P2-Vollabnahme

Die Schnittflaeche ist **vorgegeben** und planar, die Luft- und Wassergeschwindigkeiten stammen **nicht** aus einem dynamischen zweiphasigen 3D-MAC-Solver. Die Austauschregel ist nicht in die 3D-VOF-/PLIC-Advektion, MAC-Druckprojektion oder den 3D-SDF-Kapillaroperator eingekoppelt. Es fehlen eine echte 3D-Stress-/Ghost-Randbedingung, verteilte Fluss- und Oberflaechenenergie, bewegte Festkoerper/FSI und globales physikalisches Impuls-/Energiekonto. Keine neuen >=6-s-MARIN-Messungen, keine H1-H4/P1-P8-Dreigitterkonvergenz, kein UE5.8-Native-E2E. Der bestehende 6-s-MARIN-Hoehen-RMSE ist **FAIL**.

**P2 PARTIAL / phase_complete=false**. Die isolierte Paarbilanz darf nicht als vollstaendige 3D-CFD- oder P2-Abnahme ausgegeben werden.
