# P2 – lokale VOF-Normalenrekonstruktion (Teilabnahme, 31.10.2026)

**Status: P2 weiterhin INCOMPLETE.** Diese Arbeit verbindet einen read-only
Normalenschaetzer aus benachbarten Wasser-Volumenanteilen mit dem vorhandenen
beliebig orientierten PLIC-Zellmomentintegrator. Sie veraendert weder
`CoupledMacWater3D` noch die VOF-Transportgleichung.

## Implementierung

- `include/upc/p2_vof_normal_reconstruction3d.hpp`:
  `p2_reconstruct_vof_normals3d` bestimmt fuer jede teilweise gefuellte
  Fluidzelle den normalisierten **negativen** lokalen Gradienten der
  Volumenfraktion. Zentrale Differenzen nutzen zwei gueltige Nachbarn;
  an Gitterraendern und festen Hindernissen wird eine einseitige Differenz
  verwendet. Volle/leere Zellen liefern Nullnormalen.
- Unbestimmbare teilweise gefuellte Zellen (konstanter Nachbarschaftsfuellgrad)
  fuehren explizit zu `invalid_argument`. Ungueltige Masken,
  Volumenanteile, Dimensionen und Zellweiten werden abgewiesen.
- `p2_vof_reconstructed_plic_geometry3d` verwendet diese geschaetzten
  Normalen fuer den bereits vorhandenen obliquen PLIC-Geometriemomentadapter.
  Wassermasse, Schwerpunkt und Traegheit bleiben read-only.

## Native Nachweise

MSVC Release-Build mit 100/100 CTest-Suites PASS. Der neue Native-Test
`upc_p2_vof_normal_reconstruction_tests.exe` bestand mit
**13.065 Assertions** und den folgenden abgegrenzten Referenzen:

| Fall | Raster | Masse | Teilzellen | Einseitige Achsen |
| --- | ---: | ---: | ---: | ---: |
| Affiner 3D-Fuellgrad | 8³ | 4 kg | 512 | 384 |
| Affiner 3D-Fuellgrad | 12³ | 13,5 kg | 1728 | 864 |
| Scharfer obliquer Ebenenschnitt | 12³ | 13,5 kg | 268 | 96 |
| Asymmetrische feste Nachbarzelle | 3³ | 104,16 kg | 26 | 50 |

Fuer affine Felder wird die analytisch bekannte Normale
`normalize(0.018,-0.014,0.011)` an **allen** teilgefuellten Zellen
bis auf 2e-13 verglichen. Ein synthetischer scharfer Ebenenschnitt
mit `normalize(0.8,0.5,-0.3)` ergibt minimale
Richtungsuebereinstimmung (Skalarprodukt) **0,8443223135** und
mittlere Uebereinstimmung **0,9784871273**; die Normale ist dort
ausdruecklich nur eine Naeherung. An einer blockierten Nachbarzelle
wird die einseitige Differenz gegen die analytische Normale geprueft.
Dazu kommen Massenbilanz, Bezugspunktinvarianz der Traegheit,
Eingabe- und Fehlerfaelle und die Unveraenderlichkeit der Eingaben.

Reproduktion innerhalb von `D:\Projekt Engine`:
`Engine Core\UnifiedPhysicsCore\scripts\p2_vof_normal_reconstruction_full_build.cmd`
und `python "Engine Core\UnifiedPhysicsCore\scripts\p2_vof_normal_reconstruction_acceptance.py"`.
Build-, CTest- und maschinenlesbare SHA256-Nachweise:
`ENGINE TEST STUBE\runs\p2_vof_normal_*.txt` und
`ENGINE TEST STUBE\runs\p2_vof_normal_reconstruction_partial.json`.

## Explizite Grenzen / naechste Schritte

Der Gradient von Zellmittelpunkt-VOF-Anteilen ist kein geometrisch exakter
PLIC-Normalenschaetzer an scharfen Grenzflaechen. Die synthetischen
Ebenenschnitte sind keine unabhaengigen gemessenen CFD-Daten.
Insbesondere fehlen konservativer PLIC-Grenzflaechentransport,
Kontaktwinkel/Oberflaechenspannung, dynamische Zweiwege-FSI,
globale physikalische Impuls- und Energiebilanzen, externe
3D-CFD-Referenzen und ein neues Unreal-5.8-Native-E2E.
Die P2-Vollabnahme und alle P3–P8-Vollabnahmen bleiben offen.
