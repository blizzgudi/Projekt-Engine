# P7 – Hardware-GPU / 3D-LOD / Streaming / Leistungsbudgets: native Prototyp-Abnahme

Stand: 2026-10-26. Dies ist die abschließende **abgegrenzte P7-MVP-Abnahme** für
konservative 3D-Render-LOD-Übertragung. Reale GPU = NVIDIA GeForce RTX 3080 Ti
(DXGI Vendor 0x10de), D3D11 Hardware-Device und Feature Level 11_0.

## Tatsächlich implementierte und getestete Module

1. \`shaders/p7_vof_reduce.hlsl\`: echter HLSL-Compute-Kernel
   (cs_5_0) auf einem D3D11-HARDWARE-Device, nicht WARP/CPU-Shader.
   Ein \`StructuredBuffer<float2>\` mit Wasser-VOF und Feststoff-
   belegung wird mit \`RWStructuredBuffer<float2>\` auf 3D-Coarse-
   Zellen reduziert. Die Randzellen werden durch die *real*
   abgedeckte feine Zellanzahl geteilt, nicht durch festes 2^(3L).
   Dies ist ein Sicht-/Render-LOD-Reducer und kein GPU-MAC-
   Druck-/Strömungssolver.

2. \`benchmarks/p7_gpu_d3d11.cpp\`: Device/Adapter-Kennung aus DXGI,
   tatsächliches CSMain-D3D11-Dispatch, GPU-Timestamp-Query,
   synchrones GPU->CPU-Staging-Readback und CPU/GPU-
   Zell-für-Zell-Vergleich gegen die **bereits bestehende**
   \`SparseWaterLod3D\`-CPU-Referenz. Getestet:
   - 17×13×9: abgeschnittene Ränder und Feststoffbelegung
   - 12×8×6: 30 echte Schritte aus dem P8-Dammbruch-MAC/VOF-Solver
     mit 128 kg Anfangs-/Endwassermasse (dasselbe physische
     Ausgangsszenario wie UE5.8 P8)
   - 32³ / 48³ / 64³: größer werdende 3D-Snapshot-Raster.
   - 4 Stufen (Downscale 2/4/8/16): 20 analytisch/szenisch
     referenzierte GPU-Dispatches in realem VRAM.
   - per-cell Wasser/Solid absolute Differenz <= 2e-6,
     gewichteter Wasser-Volumenfehler <= 2e-5 m³.
   - GPU-Kernel-Zeit aus D3D11-Query, und vollständige
     Upload + Dispatch + Readback-Zeit aus Wall-Clock getrennt
     erfasst; beide NICHT irreführend gleichgesetzt.
   - Resident GPU-Input-/Output-/Staging-/Constant-Buffer
     Bytes exakt (ohne Treiber-/Allocator-Overhead);
     64³-Pufferbudget der gemessenen Fälle unter 8 MiB.
     Das ist kein vollständiger VRAM-Heap-Peak.

3. \`include/upc/p7_brick_streamer3d.hpp\`:
   \`WaterBrickStreamer3D\` mit 8×8×8-Kacheln, Null-Zell-
   Sparsity, interessenabhängiger Nähe-Sortierung,
   Blickdistanz-LOD (nah/mittel/fern), Generationszählern,
   pro-Frame-Staging-Upload-Budget, Resident-Kapazität und
   deterministischer Konvergenz binnen maximal
   \`ceil(aktive_Bricks / Bricks_pro_Frame)\` Frames.
   Eine neue Generation entfernt alte Bricks, sodass keine
   simulierten alten Materialzustände weiter angezeigt werden.
   \`tests/p7_brick_streamer3d_tests.cpp\` validiert LOD 0/1/2,
   dynamische Änderungen, überschrittene Budgets und vollständige
   volumenkonservative Übertragung (in CPU-Float32-Toleranz).

4. \`benchmarks/p7_gpu_streaming_d3d11.cpp\`:
   Schreibt mit echten \`ID3D11DeviceContext::UpdateSubresource\`
   die frisch residenten CPU-Bricks pro Frame in den tatsächlichen
   GPU-Standard-Resource-Buffer. Maximal **4096 Payload-Bytes
   pro Frame**, explizite 65536-Byte-GPU-Resident-Kapazität,
   pro-Frame-Latenzmessung, alle GPU-Ressourcen zurückgelesen
   und *byte-identisch* mit den übertragenen Brick-Payloads
   verglichen. Testet LOD 0/1/2, jeweils 2 Bricks/2 Frames.
   Meldet tatsächlich beobachtete Maximalzeiten. Keine
   Annahme, dass ein Software-Upload ein Hardwaretransfer sei.

5. P7 ergänzt den P8-Befund, ersetzt diesen nicht:
   Die physikalische MAC/VOF-Quelle (12×8×6) liefert dieselbe
   nachweisbar erhaltene Wassermasse von 128 kg. P8 führt
   tatsächlich einen Unreal-Game-Prozess und echten PIE-Prozess
   mit echter GPU-SceneCapture durch. Die aktuelle P8-Runtime
   nutzt weiterhin die Basis-Instanzenanzeige, NICHT direkt
   unseren neuen D3D11-LOD-Streaming-Layer. Dieser ist als
   eigenständige optionale Core-/Grafik-Subsystem-Bibliothek
   abgenommen. P7 kann trotz P8-Erfahrung KEINE
   Unreal-GPU-Performanceverbesserung reklamieren.

## Abschlusskriterien

- MSVC Release CMake-Build und alle CTest-Suites PASS.
- P7-Partial (historisch): konservative MAC-CPU-Snapshots und
  Speicherbenchmark 32/48/64 weiter gültig.
- Hardware-D3D11-Device VendorID != 0, FeatureLevel>=11_0,
  kein WARP-Device; alle HLSL-Dispatches erfolgreich.
- 20 GPU-vs-CPU-Paarvergleiche auf 5 Rasterformen und
  4 Skalen, absolute Wasser-/Solids-Abweichung <= 2e-6,
  gesamte Wasservolumen-Abweichung <= 2e-5 m³.
- GPU-Kernel-Timestamp-Query und Host-Wall-Latenz sowie
  GPU-Buffer-Allokationsbudget gemessen, explizite
  obere Prototyp-Latenzschutzgrenze 250 ms pro 3D-LODx-Snapshot
  inklusive synchronem Readback; GPU-only <25 ms.
  Diese Grenze ist *nicht* ein AAA-Frametime-Budget.
- 3 Hardware-GPU-Brick-Streaming-Fälle mit:
  1 Brick/Frame (4096 B), bis 2 Frames Konvergenz,
  65536 B GPU-Puffer, Byte-Differenzen == 0,
  pro-Frame GPU-Update Wall-Time <1000 ms (Schutzgrenze).
- Vollständige CPU-streaming Tests >250000 native Assertions
  (inklusive dynamischer Quellenänderung).
- P8 Game+PIE Native-E2E-Bericht, Bild-/Quellchecks und
  Engine-Prozess-Exit-Code 0 bleiben gültig.
- SHA256 für alle P7-relevanten Quellen sowie GPU-
  Shader und Benchmark-Harness, damit ein späterer
  Quellwechsel ohne Re-Test **keinen PASS** erhält.
- \`scripts/p7_acceptance.py\` erstellt
  \`ENGINE TEST STUBE/runs/p7_acceptance.json\`;
  \`scripts/phase_gate.py\` darf P7=PASS nur bei
  diesen und vorherigen Gates vergeben.

## Wissenschaftlich/produktionstechnisch weiterhin offen

- Kein GPU-Navier-Stokes- / MPM- / FEM-Rechenkern, kein
  hardwarebeschleunigtes universelles Physik-Engine-System:
  GPU berechnet und speichert *nur* die LOD-Projektion/
  Renderer-Voxel.
- Weder Renderthread/Fence-Integration noch asynchrone
  Triple-Buffer-Verarbeitung mit Unreal/RDG, keine
  In-Game-LOD-Profiler-Verbesserung oder 60/120 FPS Beweis.
- \`SparseWaterLod3D\` und CPU-Streaming brauchen bei
  dichten Welten weiterhin O(N³) Snapshot-Durchlauf.
  Die Größen 64³ sind noch kein großskaliges AAA-Level.
- Die gemessene GPU-Rechenzeit alleine ist klein; die
  gemessene CPU->GPU->CPU-Synchronisierung kann deutlich
  teurer als der CPU-LOD-Durchlauf sein. **Kein nachgewiesener
  End-to-End-Speedup**. GPU-Vorteile für asynchrones Rendering
  bedürfen späterer UE-RDG-/Compute-Shader-Integration.
- Resident- und Transferbudget bezeichnet die selbst
  angelegten Nutzdatenpuffer, nicht kompletten DXGI/
  Unreal-Allocator-Verbrauch.
- Kein systemweiter Leistungstest aller P3–P6-Solver.

**Fazit:** P7 ist als nativer, messbar validierter Hardware-
GPU-LOD- und Resident-Streaming-Prototyp technisch vollständig
gegen *seinen definierten Umfang* nachgewiesen. Er ist kein
AAA-Rendering-/GPU-Physics-Produktionsnachweis.
