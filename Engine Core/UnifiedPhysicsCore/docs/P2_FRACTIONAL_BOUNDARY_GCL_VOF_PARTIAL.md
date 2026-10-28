# P2 Teilnachweis: subzellgenaue offene Volumenanteile / 3D-MAC-GCL

Stand: 2026-10-28. **P2 bleibt PARTIAL, P3–P8 ebenfalls.**

## Implementierter Umfang

`include/upc/p2_fractional_boundary_vof_flux3d.hpp` ergänzt die bisherigen
binären Voxelmasken um **fraktionale, extern vorgegebene offene Zellvolumen**
`alpha_old` und `alpha_new` zwischen 0 und 1. Die Wassermenge wird als
Volumen pro **voller** Gitterzelle geführt (`water_cell_fraction`), also
nicht als auf das offene Teilvolumen normierte Sättigung.

Die diskrete geometrische Erhaltungsgleichung gilt pro Zelle:

`alpha_new = alpha_old - (dt/dx) * sum(outward_MAC_face_velocity)`.

Wasser wird ausschließlich über unmittelbar benachbarte 3D-MAC-Faces
transportiert. Bei einer Donorzelle mit offenem Anteil `alpha_old > 0`
ist die transportierte Wassermenge pro vollem Zellvolumen

`swept_volume * water_old / alpha_old`.

Das unterscheidet sich bei teilbelegten Zellen von `swept_volume *
water_old` und verhindert eine systematische Unterschätzung der
Wassersättigung. Die Summe aller ausgehenden Volumenflüsse muss
`<= alpha_old` sein. Ungültige Volumenanteile, Wasserüberfüllung,
NaN/Inf, undurchlässige Zellen, Außenrandflüsse, GCL-Verletzungen und
überschrittene Donorvolumina werden abgewiesen. Die Eingaben sind
unveränderlich; auch bei Fehlern wird kein Zustand committed.

**Wichtig:** Die freien Volumenanteile und die MAC-Face-Flüsse werden
vom Aufrufer vorgegeben. Der Operator berechnet weder die
Subvoxel-Schnittgeometrie eines Körpers noch dessen Druck-/Impuls-
Rückwirkung. Eine erfüllte GCL ist notwendig, aber kein Beweis
physikalisch korrekter Fluid-Festkörper-Kopplung.

## Native und unabhängige Referenzen

- Drei 3D-Gitter: 8³, 12³ und 16³; alle drei Achsen, beide Vorzeichen,
  je 0,25 und 0,5 Zellvolumen Sweep. 36 analytische Fälle mit
  `alpha_old=(1,0.5)`, `alpha_new=(1-c,0.5+c)`,
  `water_old=(0.8,0.2)`. Erwartet:
  `water_new=(0.8*(1-c),0.2+0.8*c)`.
- 36 weitere Fälle mit **zuvor geschlossener Empfängerzelle**:
  `alpha_old=(1,0)`, `alpha_new=(1-c,c)`,
  `water_new=(0.8*(1-c),0.8*c)`.
- 3 echte 3D-Split-Flux-Fälle: 0,25 Zellvolumen über X und Y,
  erwartete Wasseranteile `(0.3,0.25,0.2)`, exakt konservativ.
- 3 statische 3D-Zirkulationsfälle zur Rückwärtskompatibilität:
  `(1,0,0,0) -> (0.75,0.25,0,0)` bei Courant 0,25.
- Negativfälle für fehlerhafte GCL, Donor-CFL, geschlossenen Donor
  und Empfänger, Überfüllung, Domain-Grenze, NaN/Inf, falsche
  MAC-Arraygröße und Null-Zeitschritt.

Die Referenzen werden separat in
`scripts/p2_fractional_boundary_vof_acceptance.py` numerisch geprüft,
nicht nur durch Rückgabecode der C++-Tests. Native Regression:
`build/upc_p2_fractional_boundary_vof_tests.exe`. Vollständige
Regression: `ctest --test-dir build -C Release --output-on-failure`.

Nachweisdateien innerhalb von `D:\Projekt Engine`:
`ENGINE TEST STUBE/runs/p2_fractional_full_ctest_20261028.txt` und
`ENGINE TEST STUBE/runs/p2_fractional_boundary_vof_partial.json`.
Der JSON-Bericht enthält native Ausgabe, unabhängige Prüfergebnisse,
Quell-SHA-256, CTest-Log-SHA-256 und `phase_complete=false`.

## Nicht nachgewiesen / nächster Schritt

Keine aus einer bewegten 3D-Körperoberfläche automatisch abgeleiteten
Flüsse, keine 3D-Druckprojektion oder dynamische Starrkörper-Gegenkraft,
keine globale mechanische Energie-/Impulserhaltung, keine Einbindung
in `CoupledMacWater3D`, keine gemessenen externen 3D-CFD-Referenzen,
kein neues Unreal-5.8-Native-E2E für diesen Operator.

Als nächstes: tatsächliche Schnittvolumina und gesweepte Face-Volumina
aus zeitabhängiger Körperkinematik erzeugen, diese mit dem
Druck-/Geschwindigkeitsschritt koppeln und die Fluid- sowie
Körperimpulse unabhängig bilanzieren. **Keine Vollphasenfreigabe.**
