# P2 Teilnachweis: gefrorene 3D-MAC-Druckkraft als Starrkörper-Impulsantwort

Stand: 27.10.2026. **P2 PARTIAL – keine vollständige Phasenabnahme.**

## Inhalt

`include/upc/rigid_pressure_response3d.hpp` ergänzt den vorherigen
statischen 3D-Mehrvoxel-Druckkraftsensor um eine **isolierte,
konstant belastete Starrkörper-Zeitschrittantwort**. Die Kraft und das
Drehmoment kommen aus `StaticSolidPressureProbe3D::load_on_body` nach
einer tatsächlichen MAC-Projektion. Der Aufrufer muss das Druckdrehmoment
**um den aktuellen Körpermittelpunkt** anfordern.

Ein Zeitschritt mit `dt>0`, Masse `m>0` und positiver diagonaler
Weltträgheit `I` berechnet:

- `J=F*dt`, `dL=tau*dt`; `v'=v+J/m`, `omega'=omega+dL/I`.
- `x'=x+(v+v')*dt/2` (konstante Kraft, exakte Translation).
- Gegenimpuls im **nur protokollierten** Fluid-Konto: `-J`, `-dL`.
- Druckarbeit `F·(x'-x)+tau·((omega+omega')*dt/2)`.
- Änderung der translationalen/rotatorischen kinetischen Energie und
  numerisches Residuum zur Druckarbeit.
- Ungültige/NaN-/unendliche Eingaben und Überläufe werden verworfen.

**Kein echter Fluidimpuls wurde zurückgeschrieben.** Das MAC-Feld,
die Voxelmaske, die freie Oberfläche und die Körperorientierung werden
nicht aktualisiert. Die Trägheit ist absichtlich eine feste diagonale
Weltmatrix; es gibt keine gyroskopischen Terme. Wiederholte Schritte
mit eingefrorener Kraft sind ausschließlich eine numerische Referenz
und **keine** validierte bewegte Körper-/Flüssigkeits-Simulation.

## Unabhängige Referenz

`tests/p2_rigid_pressure_response3d_tests.cpp` erzeugt aus einem
analytisch vorgegebenen 3D-MAC-Geschwindigkeitsfeld mit echten
Wasser-/Festkörper-/Luft-Masken ein projiziertes Druckfeld. Für einen
voll benetzten 2x2x2-Körper, `dx=0.08 m`, Drucksteigungen
`(7,-11,5) Pa/Zelle`, ist die unabhängige Kraft
`(-0.5376,+0.8448,-0.3840) N` und das Drehmoment um den
Körpermittelpunkt Null. Testmatrix: drei Auflösungen
`8x7x6`, `12x11x10`, `16x15x14`; offene Wasser-Luft-Grenze
und geschlossene Neumann-Grenze; je zwei Druck-Offsets
`150` und `1200 Pa` (12 Fälle).

Unabhängiges Python-Gate
`scripts/p2_rigid_pressure_response_acceptance.py` berechnet
`v'=v+F*dt/m`, `x'=x+v*dt+F*dt²/(2m)`, `-F*dt`
und `W=F·(x'-x)` aus den analytischen Drucksteigungen neu.
Der native Test prüft außerdem exakte Ein-/Zwei-Halbschritt-Identität,
einen **synthetischen** nichtverschwindenden Drehmomentfall,
Arbeits-/Energieidentität, Reaktionsvorzeichen, Ballistik,
ungültige Masse/Trägheit/Zeitschritt/NaN und Überlauf.

## Nachweise und Reproduktion

Ausführen in `Engine Core/UnifiedPhysicsCore`:

```powershell
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
python scripts/p2_rigid_pressure_response_acceptance.py
python scripts/phase_gate.py
```

- Native-Test: `physics_p2_frozen_pressure_rigid_response_partial`.
- Maschinenlesbar:
  `ENGINE TEST STUBE/runs/p2_rigid_pressure_response_partial.json`.
- Vollständiges CTest-Protokoll:
  `ENGINE TEST STUBE/runs/p2_rigid_pressure_response_ctest_20261027.txt`.
- SHA256 für Quelldateien, Release-EXE und CTest-Log.
- Nur bei bestandenem Gate `partial_accepted=true`;
  `phase_complete=false` ist unveränderlich.

## Grenzen und offene P2-Kriterien

**Nicht erfüllt:** Reale Körperbewegung im Voxel-/MAC-Feld,
fluidseitige Gegenimpulsanwendung, Wasserverdrängung durch
Körperbewegung, rotierende Trägheit/Orientierung, stresskonsistente
freie Oberflächen, globale Energieerhaltung, unabhängige gemessene
3D-CFD-Referenzen und gekoppelte UE5.8-Native-E2E-Abnahme.
Die Gegenimpuls-Summe ist nur eine **buchhalterische Verpflichtung**,
kein FSI-Erhaltungstest. P2–P8 bleiben unvollständig.
