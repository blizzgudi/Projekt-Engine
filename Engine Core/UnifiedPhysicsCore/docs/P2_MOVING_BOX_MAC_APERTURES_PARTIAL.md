# P2 PARTIAL — exakte zeitgemittelte MAC-Face-Oeffnungsflaechen fuer einen bewegten 3D-Quader

Stand: 2026-10-28. **Keine Vollphasenabnahme.**

## Implementierung

`include/upc/p2_moving_box_mac_apertures3d.hpp` berechnet fuer einen
achsenparallelen, starr und linear zwischen zwei Zeitpunkten translatierten
Quader die offenen Flaechenanteile aller 3D-MAC-Faces. Ergebnis:
`u_open`, `v_open`, `w_open` als **zeitgemittelte geometrische
Apertur** in [0,1]. Das sind weder Face-Geschwindigkeiten noch
transportierte Volumina. Die Geometriepruefung verwendet
`P2MovingBoxOpenVolumes3D::sample` und liefert zusaetzlich die
subzellgenauen offenen Zellvolumina `alpha_old/alpha_new`.

Fuer ein Face ist die momentan blockierte Flaeche
`I(box_min_normal < face_plane < box_max_normal) * overlap_t1/dx *
overlap_t2/dx`. Die Zeitintervalle werden an allen Zeitpunkten
geteilt, an denen Quaderkanten eine Face-Ebene oder eine
transversale Zellkante passieren. Auf jedem offenen Intervall
ist die blockierte Flaeche ein Polynom hoechstens zweiten Grades.
Zwei innere Gauss-Legendre-Punkte integrieren dieses Polynom exakt
(bis auf Floating-Point-Rundung), auch bei normalem Ein-/Austritt
und gleichzeitiger tangentialer Bewegung.

## Native Referenzen

`tests/p2_moving_box_mac_apertures3d_tests.cpp` testet die drei
MAC-Normalachsen, 8^3/12^3/16^3 Gitter und beide
Bewegungsrichtungen. Unabhaengige geschlossene Referenzen:

- Normaler Durchtritt: zwei Faces 0.25 und 0.75 offene Zeitanteile.
- Tangentiale halbe Zelltranslation: linearer Mittelwert 0.5.
- Diagonale Translation in beiden Face-Tangenten: quadratischer
  Integralwert 35/48.
- Gleichzeitig normale und tangentiale Bewegung: stueckweiser
  Integralwert 37/64.
- Ruhender subzelliger Quader: statischer offener Face-Anteil 7/16.\n- Gitterausgerichtete Festkoerpergrenzen: offene Apertur 0 auf
  beiden Grenz-Faces; tangentiales Gleiten ergibt 1/2.

Jede vollstaendige Face-Array-Ausgabe wird auf [0,1] geprueft;
Zeitumkehr `old <-> new` muss dasselbe zeitgemittelte
Aperturfeld ergeben. Hinzu kommen Domain-Grenzen,
Gesamtvolumen, negative Tests fuer ungueltige
Geometrie/Gitter/Zeit/CFL und Unveraenderlichkeit der Eingaben.

## Nachweis und Grenzen

MSVC Release, vollstaendige CTest-Suite, Native-Assertion-Zaehler,
separate Python-Referenzpruefungen, Quell-SHA256 und Log-SHA256:
`scripts/p2_moving_box_mac_apertures_acceptance.py` und
`ENGINE TEST STUBE/runs/p2_moving_box_mac_apertures_partial.json`.
Die konkreten Zaehler sind dem maschinenlesbaren Report zu entnehmen.

**P2 bleibt PARTIAL; P3-P8 bleiben PARTIAL.** Die Aperturen sind
geometrische Koeffizienten fuer einen kuenftigen Cut-Cell-Druck-/
Volumenflussoperator. Sie erzwingen allein weder die lokale GCL
noch No-Penetration an der bewegten Wand. Es gibt keine
berechneten MAC-Fluesse, keinen Druck, keine Wasser-/Starrkoerper-
Gegenimpulsbilanz und keine globale Energieerhaltung.
Nur starre achsparallele Quader ohne Rotation/Deformation.
Unabhaengige 3D-CFD-Messvalidierung und ein neuer
Unreal-5.8-Native-E2E-Nachweis fehlen.
