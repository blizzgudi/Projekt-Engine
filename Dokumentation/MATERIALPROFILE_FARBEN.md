# Farb- und Materialprofile im Unified Physics Core

Stand: 2026-10-26. Projekt: D:\Projekt Engine.

## Grundentscheidung

Die einzelnen Voxel speichern **keine eigene Farbe**, sondern
weiterhin nur `material_id`, Füllgrad, Temperatur und Schaden.
Zentrale Materialprofile legen **einmal pro Material-ID** die
Anzeigeparameter fest. Durch Ändern eines Profils verändert
sich die Darstellung aller Voxel mit derselben ID.

## Wo liegt die Implementierung?

- Physikalische Materialdaten und Material-IDs:
  `Engine Core/UnifiedPhysicsCore/include/upc/core.hpp`:
  `MaterialLibrary`, `Material`, `Voxel`.
- Neue visuelle Materialprofile:
  `Engine Core/UnifiedPhysicsCore/include/upc/material_profile.hpp`:
  `MaterialProfile`, `MaterialAppearance`,
  `MaterialProfileLibrary`; voreingestellte Profile für
  Wasser (RGB 0.06/0.35/0.82), Eis, Sand und Metall.
- Native Testdatei:
  `Engine Core/UnifiedPhysicsCore/tests/material_profile_tests.cpp`.
- Vorlage für **zukünftig persistente** Materialdaten:
  `Materialdatenbank/material_profiles.example.json`.
  Achtung: momentan nur Vorlage, **kein JSON-Lader im Core**.
- Vorhandenes Unreal-Rendering:
  `ENGINE TEST STUBE/unreal/ENGINE_TEST_STUBE/Plugins/EngineMaterialLab/Source/EngineMaterialLab/Private/EngineMaterialLabModule.cpp`.
  Es weist im heutigen Stand **noch kein** Profile-Shader-
  Material zu und zeigt das Blau daher nicht automatisch.

## Wie funktioniert es?

1. Einmal `MaterialLibrary` mit Dichte, Phase usw. registrieren.
2. `MaterialProfileLibrary profiles(physics_materials)` anlegen
   und per `profiles.set(make_water_profile(1))` registrieren.
3. Jedes Wasser-Voxel enthält weiter `material_id = 1`.
4. Der Renderer löst Material-ID -> visuelles Profil auf
   und benutzt RGB, Opazität, Rauheit, Metallizität.
5. Profiländerungen wirken auf alle gleichnamigen Voxelflächen.
   Voxelmasse, Physik und VOF-Strömung bleiben unverändert.
6. Der spätere Editor soll die JSON-Profile laden, speichern
   und bei Änderungen zur Laufzeit aktualisieren.

## Nachweis

Das isolierte native Testprogramm wurde unter Visual Studio
2026 Build Tools / C++17 Release kompiliert und ausgeführt:
**PASS, 17 Prüfungen**. Es verifiziert Wasser-Blau,
4096 Voxel mit gemeinsamer ID, zentrale Farbänderung,
unveränderte physikalische Masse und abgewiesene ungültige
RGB-/Shaderwerte.

## Noch offen, bevor Wasser in Unreal sichtbar blau wird

- Unreal-Material bzw. Material-Instance mit eindeutigen
  Parametern `BaseColor`, `Roughness`, `Metallic`, `Opacity`.
- Binding `MaterialProfileLibrary` -> Unreal Shader-Parameter.
- Getrennte Darstellung von transparentem Wasser / Eis /
  Sand / Metall und einfache Materialeditor-Eingabemaske.
- JSON-Persistenz samt Lade-/Speicherfunktion (Vorlage existiert).
- Visueller Unreal-Game+PIE-Test auf echte blaue Wasseroberflächen.

Wichtig: Diese neue Profil-Ergänzung fasst weder vorhandene P0–P8-
Physikdateien an, noch ersetzt sie UE-Material-Shader. Sie ist ein
getesteter neuer **zentraler Metadaten-Baustein**, kein fertiger
Material-Lab-Editor.
