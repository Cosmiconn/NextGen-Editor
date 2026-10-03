# Renderer – Stand und Datenherkunft

**Stand:** 03.10.2026 · Bezug: Fiesta Online NA2016-Client (Fixtures der Karte Roumen `Rou`)

Dieses Dokument trennt strikt zwischen **aus Fiesta-Daten gelesen**, **verifiziert** und
**Editor-Näherung**. Eine Näherung ist eine bewusste Darstellungsentscheidung des Editors, keine
Behauptung über die Client-Formel.

## 1. Pipeline

1. `HeightmapRenderer::BeginScene` – Szene in ein Multisample-FBO (Standard 4× MSAA, begrenzt auf
   `GL_MAX_SAMPLES`, Rückfall auf 1×, wenn das FBO unvollständig ist).
2. Terrain (HTD, bis 24 Layer in Gruppen à 8), danach Objekt-Platzhalter, NIF-Meshes (Placements,
   SHMD Sky/Water/GroundObject, NPCs) in dasselbe FBO.
3. `EndScene` – MSAA-Auflösung per `glBlitFramebuffer`, danach optional Glow-Nachbearbeitung.
4. Alle Renderer, Picking, Gizmo und Overlays nutzen **eine** Projektion:
   `OrbitCamera::ProjectionMatrix(aspect)` (Perspektive mit FOV 0,9 rad oder orthografisch).

## 2. Kartenlokale Fiesta-Daten

| Daten | Quelle | Status |
|---|---|---|
| Terrain-Vertexfarben | `.ini` `#VerTexColorTexture` (Rou: `Rouvertexcolor2.bmp`, 257×257 = HTD-Vertexgitter) | **gelesen**, als Vertexattribut im Terrain-Puffer |
| Bodenlicht an/aus | `<Karte>.conf` `[WorldSetting] Ground_DL_Enable` | **gelesen**, schaltet gerichtetes Licht auf dem Terrain |
| Glow | `<Karte>.conf` `[GlowScreenEffect]` Glowness/BlendFactor/GaussFactor/DownScaling/NumBlurring | **gelesen**, Nachbearbeitung als Näherung |
| unbekannte `.conf`-Einträge | alle Abschnitte | **roh erhalten** (`MapRenderConfig::sections`) |
| Umgebungslicht | `.shmd` `GlobalLight` (+ `DirectionLightAmbient`) – Rou: 0,792 grau | **gelesen**, Terrain + NIF |
| Sonnenfarbe | `.shmd` `DirectionLightDiffuse` – Rou: 1,0 / 0,976 / 0,902 (warm) | **gelesen**, Terrain + NIF |
| Hintergrund | `.shmd` `BackGroundColor` – Rou: 0 / 0,502 / 1,0 | **gelesen**, Löschfarbe des Viewports |
| Nebel | `.shmd` `Fog` (Tiefe + Farbe, Rou: 0,57 · 0,071 / 0,541 / 0,929) und `Frustum` (5000) | **gelesen**, Nebel als Näherung (s. u.) |

Kern: `core/legacy/MapRenderSettings` + `test_map_render_settings` (gegen die echten Dateien
`Rou.conf` und `Rouvertexcolor2.bmp`); SHMD-Umgebung: `src/app/SceneLighting.hpp` +
`test_scene_lighting` (gegen die echte `Rou.shmd`).

### Verifiziert an Roumen

- Zeilenkonvention der Vertex-Color-Bitmap = Blend-BMPs (Dateizeile y == Gitterzeile z, kein Flip,
  CHANGELOG [0.44.27]); der Test vergleicht beide Leser zeilenweise.
- Die Bitmap ist mit der Heightmap deckungsgleich: Die letzte Bitmapzeile ist links über genau
  21 Vertices dunkel (35,72,103) und rechts hell – dieselbe Teilung wie Meer (~) zu Plateau (=)
  in der letzten HTD-Zeile. Die übrigen dunklen Flächen liegen unter dem Pflaster-GroundObject
  (`Rou_ground2_CD.nif`), passend zu einer gebackenen Beleuchtung mit Objektschatten.
- Mittelwert R/G/B 133/154/178: kühler Grundton (Himmelslicht), kein neutrales Weiß.

### Editor-Näherungen (nicht aus Client-Code belegt)

- Mit SHMD-Umgebung (Standard): wie feste Funktionsbeleuchtung, auf 1 gesättigt.
  Terrain: `Textur × Vertexfarbe × min(1, Umgebung + Sonne·N·L)` (bei `Ground_DL_Enable=FALSE`
  ohne Sonnenanteil); NIF: `Oberfläche × min(1, Material-Ambient·Umgebung + Sonne·N·L)` +
  Spekular + Emission.
- Ohne SHMD-Umgebung (z. B. reine NIF-Karten): bisherige Editorformeln (`0,35 + 0,65·N·L` bzw.
  mit Vertexfarbe `0,62 + 0,55·N·L`).
- Lichtrichtung fest (-0,4, -1, -0,3) – die Daten enthalten nur Farben, keine Richtung.
- Nebel: linear vom Auge aus, Start = `Frustum × (1 − Tiefe)`, Ende = `Frustum` (Lesart der Tiefe
  wie Gamebryos `NiFogProperty`-Depth). Im Spieltest immer aktiv, im Editor optional (bei großem
  Kameraabstand würde er sonst die ganze Karte einfärben), in Achsenansichten aus.
- Glow: auf 1/`DownScaling` verkleinern, `NumBlurring` × separierbarer 9-Tap-Gauss
  (σ = 2·`GaussFactor`), Komposition `Szene + Glow · Glowness · BlendFactor`.

Alle drei sind im Viewport-Menü **Anzeigen → Darstellung** einzeln abschaltbar.

## 3. Darstellungsoptionen (Viewport → Anzeigen → Darstellung)

- Ansichtsmodus: Beleuchtet · Unbeleuchtet (nur Textur/Material) · Nur Licht/Vertexfarbe · Normalen
  (Terrain- und NIF-Shader).
- Terrain-Vertexfarben, Kartenlicht (SHMD), Hintergrundfarbe (SHMD), Nebel, Glow,
  Kantenglättung (Aus/2×/4×/8×), Drahtgitter.
- Ansicht: Perspektive oder orthografische Achsenansichten (Oben/Süd/Nord/West/Ost). In Achsen-
  ansichten verschiebt Ziehen, das Rad zoomt; Drehen kehrt zur Perspektive zurück.

## 4. Offene Punkte (ehrlich)

- Kein Abgleich gegen Client-Screenshots – dafür werden Referenzbilder aus dem echten NA2016-
  Client benötigt (gleiche Kameraposition). Die Fixtures enthalten zudem nicht alle Roumen-
  Texturen/NIFs (`L3_RE.dds`, `L7_D.dds`, `grass_01.dds` und mehrere `resmap/nifs/Common/*`
  fehlen und erscheinen grau bzw. als Platzhalter).
- Lichtrichtung: in SHMD/CONF nicht enthalten, daher fest. `NiFogProperty` in einzelnen NIFs
  wird weiterhin nur übersprungen (der Kartennebel kommt aus der SHMD).
- Proprietäre Shader, `NiTextureEffect`, `APPLY_HILIGHT2` und Partikelsimulation – siehe
  `docs/LEVEL_EDITOR_PARITY.md` §4.
- Schatten werden nicht berechnet (die gebackenen Vertexfarben enthalten aber Terrain-Schatten).
