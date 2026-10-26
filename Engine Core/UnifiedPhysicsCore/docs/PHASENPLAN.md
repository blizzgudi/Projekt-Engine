# Projekt Engine — Unified Physics Core / Phasen

Stand: 2026-10-26. Status bezieht sich auf den realen Code, nicht auf Ambitionen.

| Phase | Inhalt | Akzeptanzkriterium | Stand |
|---|---|---|---|
| P0 | Repo/Ordnerstruktur, C++17-Build, Material-IDs, 3D-Voxelzustand, Tests | MSVC kompiliert, Tests gruen, Git commit | in Arbeit |
| P1 | Konservativer Basis-Wassersolver als Nachweis | Dammbruch, Druck, stabile Masse, Hindernisse | als 2D shallow water implementiert; QA offen |
| P2 | Echte volumetrische 3D-Wassertechnik erforschen und auswaehlen (MAC/FLIP/APIC/MPM) | Vergleich mit Referenzfaellen, Speicherbudget, 3D-Kollision | geplant |
| P3 | Gekoppelte Material- und Volumenphysik | Wasser sickert in poroesen Boden, Krafteinwirkung, Energiebilanz | geplant |
| P4 | Thermodynamik und Phasenwechsel | validierte Waerme-/Massenerhaltung; Wasser/Eis/Dampf | geplant |
| P5 | Festkoerper, Bruch, granulare Stoffe | Belastungs-/Bruchtests und stabile Kontakte | geplant |
| P6 | Dynamische Materialmischungen und Regeln | Mischung mit Massenbilanz, reproduzierbare Reaktionen | geplant |
| P7 | Sparse Chunks, LOD, GPU-Prototyp | CPU/GPU-Vergleich, Skalierung, Latenzgrenzen | geplant |
| P8 | UE5.8-Anbindung ueber genau einen Adapter | Editor-Reload, Runtime-Test, Rendering/E2E | geplant |

## Architekturregeln
1. D:\\Projekt Engine ist Projektwurzel. Keine neuen Root-Ordner auf D:\\.
2. Eigenstaendige C++-Bibliothek. Unreal ist Verbraucher, keine Pflichtabhaengigkeit.
3. Gemeinsame SI-Einheiten, Material-IDs, vereinbarte Phasen und Zeitsteuerung.
4. Wenige Methoden intern, aber keine universelle Wunderformel fuer jede Materialphase.
5. Tests pruefen Erhaltung, Grenzwerte, Stabilitaet, Referenzfaelle und reale Laufzeit.
6. Nie aus einem 2D-Wassertest auf korrektes 3D-Wasser, Boden oder AAA-Spielqualitaet schliessen.
7. GitHub: privates separates Repo \`Projekt-Engine\` bevorzugt; GitHub-Remote erst nach tatsaechlich bestaetigter Erstellung eintragen. Bestehendes AI-Game-Studio-Repo nicht veraendern.
