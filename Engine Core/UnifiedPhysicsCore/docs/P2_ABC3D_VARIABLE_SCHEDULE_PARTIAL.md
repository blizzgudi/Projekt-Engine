# P2 Teilabnahme: variable 3D-ABC-MAC/PLIC-Zeitschritte mit Rundungsinventur

Stand: 01.11.2026. **P2 PARTIAL, P3-P8 INCOMPLETE.**

## Umfang

Der neue opt-in-Adapter 'include/upc/p2_viscous_mac_plic_roundoff_schedule3d.hpp'
verkettet die bestehende periodische 3D-MAC-Nichtlinearadvektion, explizite
Viskositaet, Druckprojektion und geometrischen PLIC-Transport mit
**unterschiedlichen Zeitschritten und Viskositaeten**. Er entfernt nur
VOF-Endpunkt-Rundungen innerhalb acht 'double'-Maschinen-Epsilons und
erfasst die Korrekturen getrennt von den Transportresiduen. Jedes Intervall
hat eine eigene Volumen-/Korrektur-/Divergenzdiagnose, und die
Gesamtsequenz hat kumulative Substep-Budgets. Echte unaufloesbare
PLIC-Geometrien bleiben Fehler. Die Eingabe wird nicht veraendert.

Die optionale Rundungsbereinigung wird vor dem ersten Schritt und nach
jedem Schritt angewendet. Anfangsbereinigung ist im Gesamtinventar
enthalten; die Intervallberichte erfassen nur die Korrektur nach dem
jeweiligen Schritt. Es gibt **keine** kuenstliche Massenerhaltung durch
grossflaechige Neuverteilung und keine Reparatur physikalisch fehlerhafter
Geometrie.

## Reproduzierbare Ausfuehrung

Im Verzeichnis 'D:\Projekt Engine\Engine Core\UnifiedPhysicsCore':

'cmd /d /c scripts\p2_abc3d_variable_schedule_build.cmd'

'python scripts\p2_abc3d_variable_schedule_acceptance.py'

Das CMD-Skript ruft MSVC C++17 '/O2 /W4', den eigenstaendigen Native-Test
und die 107 CTest-Suites inklusive des neuen, dauerhaft registrierten Regressionstests auf. Das Python-Gate liest die
wirklich erzeugten Logs, prueft Zahlen und SHA256-Fingerprints und schreibt
'D:\Projekt Engine\ENGINE TEST STUBE\runs\p2_abc3d_variable_schedule_partial.json'.

## Analytische Referenz und echte Messergebnisse

Die exakte kontinuierliche Arnold-Beltrami-Childress-Loesung hat
'u(t)=u(0)*exp(-4*pi*pi*Integral(nu(t)dt))' fuer die gewaehlte
periodische 1-m-Domaene. Getestet werden 12^3 Zellen, ein 3D-ABC-Feld
mit allen drei Geschwindigkeitskomponenten, ein 0.25-m3-Wasserslab,
fuenf Intervalle mit 'dt=[.012,.018,.020,.014,.036] s' und
'nu=[.002,.003,.004,.0025,.0035] m2/s', insgesamt 0.1 s.

Echter MSVC-Release-Native-Output:
- Relativer ABC-L2-Geschwindigkeitsfehler: **0.0062348056125263252**
- Maximale Geschwindigkeitsabweichung: **0.00051374213881283382 m/s**
- Numerisches kinetisches Energieratio: **0.96327611486810383**
- Analytisches Energieratio: **0.97512732142637137**
- Endvolumen: **0.25000000000000022 m3**
- Gesamt-Massenledger-Residuum: **-5.426947673759166e-18 m3**
- Absolute Endpunktkorrektur: **6.6518676404497197e-18 m3** in 88 Zell-Snaps
- Maximale Divergenz: **7.2292432456988465e-15 s^-1**
- Fuenf viskose und fuenf PLIC-Substeps
- Zusammensetzen aus einzelnen Intervallen: **max_diff=0**
- Konstante Fuenf-Schritt-Sequenz gegen den vorherigen Adapter: **max_diff=0**
- Native-Test: **6955 Assertions PASS**
- Bestehende CTest-Regression: **107/107 PASS**

Negativtests pruefen u.a. leeren Plan, ungueltige Zeit/Viskositaet,
nichtfinite Eingaben, kumulative Budgetueberschreitung und eine echte
unaufloesbare PLIC-Grenzflaeche. Bei Fehlern wird kein Teilergebnis als
Abnahme ausgegeben.

## Grenzen

Die Referenz ist eine **analytische** 3D-Navier-Stokes-Loesung,
keine unabhaengige gemessene CFD-Referenz. Das Modell bleibt ein
periodischer Konstantdichte-Solver mit passivem Wasseranteil, nicht
spannungskonsistente Zweiphasenstroemung. Die numerische Energie
dissipiert staerker als die exakte Referenz; das ist dokumentiert,
nicht als globale Energieerhaltung gewertet. Es gibt keine dynamische
Zweiwege-Fluid-Festkoerper-Kopplung und keinen neuen UE5.8-Native-E2E
fuer diesen Adapter. **P2-P8 bleiben vollstaendig offen.**
