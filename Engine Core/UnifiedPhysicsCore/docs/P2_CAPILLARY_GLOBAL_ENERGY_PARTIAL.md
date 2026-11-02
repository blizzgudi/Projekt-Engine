# P2 Teilnachweis: 3D-Kapillarenergie im echten bewegten Zweiphasen-MAC/VOF

Stand 02.11.2026. **PARTIAL, nicht P2-Vollabnahme.**

## Implementierung
`include/upc/p2_two_phase_capillary_global_energy3d.hpp` erfasst
vor und nach einem echten optionalen Kapillar-/MAC-PCG-/VOF-/konservativen
Zellimpuls-/Dual-MAC-Remap-/Volltensor-/PCG-Schritt die beiden
**physikalischen Phasenmassen**, die SI-Zellschwerenergie und die
dichtegewichtete staggered-MAC-Face-Kinetik. Die Grenzflaechenenergie
ist explizit als **diffuser Proxy** `sigma * integral |grad(alpha)| dV`
gekennzeichnet. Sie ist keine geometrische PLIC-Flaeche und keine
Young-Laplace-/Spannungsrandbedingungsabnahme.

Der neue Ledger meldet die Aenderungen von Kinetik, Potential und
Grenzflaechenenergie separat. Er addiert ausschliesslich die
nachgewiesene letzte viskose/PCG-Subschritt-Dissipation zur gemessenen
Gesamtaenderung. Der verbleibende globale Energiefehler wird **nicht**
weggerechnet oder als konserviert etikettiert. Fruehere PCG-Schritte,
Upwind-Advektion, Zell-zu-MAC-Remap, diffuse Kapillararbeit und
Wandreaktionen sind darin noch nicht vollstaendig physikalisch bilanziert.

## Reproduktion
Im Projekt `D:\Projekt Engine`:
1. MSVC x64 `vcvars64.bat` laden.
2. `cmake -S "Engine Core/UnifiedPhysicsCore" -B "Engine Core/UnifiedPhysicsCore/build" -DCMAKE_BUILD_TYPE=Release`
3. `cmake --build "Engine Core/UnifiedPhysicsCore/build" --config Release --parallel 4`
4. `ctest --test-dir "Engine Core/UnifiedPhysicsCore/build" -C Release --output-on-failure`
5. `python "Engine Core/UnifiedPhysicsCore/scripts/phase_gate.py"`
6. `python "Engine Core/UnifiedPhysicsCore/scripts/p2_capillary_global_energy_acceptance.py"`

Originale Windows-Protokolle und SHA256 stehen unter
`ENGINE TEST STUBE/runs/p2_capillary_global_energy_*`.
Die unabhaengige Python-QA wiederholt das echte Native-EXE, prueft die
analytische lineare 3D-VOF-Flaeche auf drei Gittern, die Energiepartition,
die echte nichtverschwindende Energieluecke und sechs manipulierte
Protokolle. Ungueltiges VOF, nichtfinite Geschwindigkeit, NaN-Schwerkraft,
negatives Sigma und unteriterierte PCG muessen fehlschlagen; bei
Solverfehlern bleibt der Zustand unveraendert.

## Offene P2-Kriterien
Keine stresskonsistente scharfe Wasser-Luft-Grenzflaeche, keine
Young-Laplace-Gitterkonvergenz, keine vollstaendige globale mechanische
Energie-/Impulsbilanz, keine dynamische bidirektionale FSI und kein
bestandenes MARIN/SPHERIC H1-H4/P1-P8 >=6s-Dreigitter-Mess-E2E.
Der alte statische Tropfen zeigt weiterhin steigende Spurious Currents.
Ein neuer UE5.8-Native-E2E fuer diesen Zweig fehlt.
**Phase-Gate P2 bleibt PARTIAL, ALL_COMPLETE=False.**

## Gemessener Native-Befund (6^3, dx=0.15 m, dt=0.0005 s)

- Gesamtproxy vorher: 7.070314302942335 J; nachher: 7.0703141137486245 J.
- Grenzflaechenenergie: -3.6330524144645437e-9 J; Kinetik: +1.6822043238346692e-7 J; Gravitation: -3.5378108975692157e-7 J.
- Nur letzte viskose/PCG-Dissipation: +1.0547159517697856e-14 J.
- **Offene globale Energieluecke: -1.8919369992871722e-7 J** (kein Konservierungs-PASS).
- Wasser-Massenrest: +1.7053025658242404e-13 kg; Luft: -6.6613381477509392e-16 kg.
- Native Assertions: 39, kompletter Windows CTest 146/146 PASS, sechs manipulierte QA-Traces verworfen.
