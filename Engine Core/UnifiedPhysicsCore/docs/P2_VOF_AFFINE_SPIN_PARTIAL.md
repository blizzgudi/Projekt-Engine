# P2 Teilabnahme: MAC/VOF-Affine-Spin (31.10.2026)

## Physikalischer Zweck
Die vorhandene zellzentrierte VOF-Drehimpulsdiagnose behandelt Wasser in jeder
Zelle als Punktmasse. Selbst bei exakt vorgegebener Starrkörperrotation fehlt
dadurch der Eigendrehimpuls innerhalb der Zelle. Der neue optionale
schreibgeschützte Adapter `vof_affine_spin_inventory3d` ergänzt
`m * dx² / 12 * curl(v)` je Wasserzelle. Für affine Rotation
`v=omega×r+V` ist `curl(v)=2 omega`; die Korrektur stimmt deshalb exakt
mit dem intrinsischen Trägheitstensor `I=m dx²/6` überein.

Der Adapter liest echte gestaffelte MAC-Face-Geschwindigkeiten, rekonstruiert
Zellmittel und tangentiale Gradienten (innen zentral, außen einseitig).
Das bestehende Uniform-Cell-Inertia-Modell liefert Masse, Schwerpunkt,
Impuls und Trägheit; der neue Adapter liefert außerdem den korrigierten
Drehimpuls um Ursprung und Schwerpunkt sowie den isolierten Spinbeitrag.

## Unabhängige Referenzen und tatsächliche Prüfungen
- Vollständig gefüllter Würfel, Seitenlänge 0,4 m, Dichte 1000 kg/m³:
  Masse 64 kg, `I_COM = 64 * 0.4² / 6 = 1.706666666666667 kg m²`.
  Bei `omega_z=1.3 rad/s` folgt `L_COM,z=2.218666666666667 kg m²/s`.
  Drei unabhängige Raster mit 8³, 12³, 16³ Zellen.
- Translation, dreiachsige Rotation und nichttrivialer Bezugsursprung;
  der Eigendrehimpuls ist ursprungsunabhängig.
- Asymmetrische Teilfüllungen auf 8³ und 12³: Vergleich gegen separate
  2×2×2-Gauß-Legendre-Volumenintegration der *analytisch* vorgegebenen
  affinen Geschwindigkeit, einschließlich Winkelimpuls um COM und Ursprung.
- Negativtests für überfüllte, negative und Festkörper-überlappende
  VOF-Zellen sowie nichtendlichen Bezugsursprung.
- Reproduzierbar: `scripts/p2_vof_affine_spin_full_build.cmd` (MSVC,
  kompletter Release-Build und CTest) und
  `scripts/p2_vof_affine_spin_acceptance.py` (numerische Kontrollen,
  SHA256-Fingerprints).
- Protokolle und JSON unter `ENGINE TEST STUBE/runs/p2_vof_affine_spin_*`.

## Gültigkeitsgrenzen
**P2 bleibt PARTIAL.** Diese Diagnose bewegt kein Wasser und keine
Festkörper. Die angenommene gleichmäßige Teilzellendichte ist **keine**
rekonstruierte VOF-Grenzfläche (kein PLIC). Geschwindigkeiten in trockenen
Nachbarzellen können die Curl-Näherung verfälschen. Der Ansatz liefert für
affine Testfelder exakte subzelluläre Winkelimpulse, aber keinen
konservativen Drehimpuls-Transport, keine Spannungsrandbedingung an
bewegten freien Oberflächen, keine Zweiwege-FSI-Gegenreaktion und keinen
externen 3D-CFD-Messnachweis. UE5.8 Native-E2E ist für diesen Adapter
nicht neu ausgeführt worden. P3–P8 bleiben ebenfalls offen.
