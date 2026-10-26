# Unified Physics Core — Architektur v0.1

**Status:** C++17 CPU-Prototyp. Bestehender Build/Testbeweis fuer MSVC. Keine Unreal Integration.

**UnifiedPhysicsCore** ist die gemeinsame Eintrittsstelle, haelt eine MaterialLibrary,
ein dichtes VoxelGrid und beliebig viele ShallowWater2D-Domaenen.
`step(dt)` koordiniert momentan ausschliesslich Wasserdomaenen.

**MaterialLibrary** speichert gemeinsam nutzbare IDs, Phasen, Dichte und
Viskositaets-Metadaten. Die Materialwerte bleiben nach Registrierung unveraenderlich
ueber die oeffentliche API. Keine Reaktionsdatenbank vorhanden.

**VoxelGrid** speichert material_id/fill/temperature_k/damage pro 3D-Zelle.
Die Temperatur und der Schaden werden in dieser Version nicht simuliert.
Es findet noch KEIN Datentransfer in den Wassersolver statt.

**ShallowWater2D** verwendet die tiefengemittelten Erhaltungsgleichungen
mit Rusanov-Fluss, reflektierenden Rand- und Hindernisbedingungen, adaptiver
CFL-Zeitschrittunterteilung und Oberflaechenhoehe pro 2D-Zelle.
Dichte beeinflusst berechnete Masse und hydrostatischen Druck, ist jedoch
noch nicht Teil einer Mehrphasen-Kopplung. Viskositaet ist noch ohne Wirkung.
Kein 3D Navier-Stokes, keine Blasen, Spritzer, vertikalen Stroemungen oder FSI.

**Grenzen:** homogener flacher Boden, keine komplexe Topographie, keine
Viskositaet, keine genaue experimentelle Fluidvalidierung; rein numerische
Basis-Regressionstests. Falls diese Grenzen neue Solver erfordern, geschieht
die Erweiterung unter derselben Core-API ohne Unreal-Abhaengigkeit.

**Sicherheit:** Keine plattformfremden Netzwerkfunktionen oder fremde DLLs.
Projekt schreibt nur unter D:\Projekt Engine. Git ignoriert Build- und
Laufzeit-Outputs.
