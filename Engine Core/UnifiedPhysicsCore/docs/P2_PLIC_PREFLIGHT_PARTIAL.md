# P2 Teilnachweis: periodische 3D-PLIC-Preflight-Diagnostik (16³)

**Status: P2 PARTIAL, keine Vollphasenabnahme.**

## Ziel

Die bisherige 16³-ABC-Galilei-Simulation mit realer Wasser-Luft-Grenzfläche
scheiterte mit `Unresolved periodic PLIC interface`. Der zuvor behobene
Reinphasenfall darf nicht als Lösung für echte Mischzellen gelten.

`include/upc/p2_plic_preflight3d.hpp` stellt eine **rein lesende**
Vorprüfung der Zellfüllgrade bereit. Sie verwendet dieselbe periodische
zentrierte Differenz und denselben Grenzwert (1e-12) wie
`p2_periodic_plic_normals3d`. Sie zählt aufgelöste Grenzflächenzellen,
unaufgelöste geometrische Zellen und unaufgelöste Füllgrade unmittelbar
neben 0/1 (64 * double epsilon). Die erste unaufgelöste Zelle wird
mit 3D-Index, Füllgrad und Gradient zurückgegeben.

Eine als Rundungskandidat klassifizierte Zelle wird **nicht**
automatisch korrigiert. Eine geometrisch mehrdeutige Zelle erhält
**keine** erfundene Normale. Der Produktionssolver bleibt unverändert
und verweigert weiterhin die Fortsetzung bei unaufgelöster Grenzfläche.

## Reproduzierbare Prüfung

Aus Projektwurzel:

```cmd
cmd /d /c scripts\p2_plic_preflight_build.cmd
python scripts\p2_plic_preflight_acceptance.py
python scripts\phase_gate.py
```

Der MSVC-C++17-Release-Nativtest `tests/p2_plic_preflight3d_tests.cpp`
prüft 16³-Reinphasen, eine planar teilgefüllte Grenzfläche,
die periodische Naht, eine echte symmetrische geometrische Mehrdeutigkeit,
einen 1-ULP-Endpunktkandidaten, Unveränderlichkeit der Eingabe und
Negativfälle. Die Produktions-Normalenrekonstruktion muss weiterhin
beide unaufgelösten Fälle ablehnen. CTest integriert den Nativtest.

Nachweise: `ENGINE TEST STUBE/runs/p2_plic_preflight_*.txt` und
`ENGINE TEST STUBE/runs/p2_plic_preflight_partial.json` mit SHA256.

## Grenzen / nächster Schritt

Die Vorprüfung untersucht den **gegebenen** Füllzustand, nicht die
internen Zwischenstufen des Strang-Splittings. Der 16³-ABC-Lauf mit
bewegter Wasser-Luft-Grenzfläche bleibt offen; die Ursache ist mit einer
Stufeninstrumentierung und unabhängigen geometrischen Referenzen zu
untersuchen. Auch stresskonsistente Zweiphasenströmung, bewegliche
Körper/Impulsrückwirkung, unabhängige 3D-CFD-Messdaten, globale
Energie-/Impulserhaltung sowie P3–P8-Vollabnahmen sind offen.
