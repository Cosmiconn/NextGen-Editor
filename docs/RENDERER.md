# Renderer – Stand und Datenherkunft

**Stand:** 16.10.2026 · Bezug: Fiesta Online NA2016-Client (Fixtures der Karte Roumen `Rou`, vollständiger
`resmap`-Ordner des Nutzers für den Abgleich, nicht eingecheckt)

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

Beleg für die Farbdeutung von `Fog` 2–4 über vier echte Karten: Bei SwaDn01 ist die Nebelfarbe
exakt `BackGroundColor`, bei Bera bis auf eine 8-Bit-Stufe (199/255 zu 198/255), bei Roumen nahe
daran; nur der Dungeon Mem_UA weicht ab (dunkles Blaugrün auf fast schwarzem Grund). SwaDn01 ist
die SHMD-Variante ohne Licht-Fußteil (`test_scene_lighting` mit allen vier Dateien).

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
  ohne Sonnenanteil); NIF: `Textur × min(1, Emission + Material-Ambient·Umgebung +
  Material-Diffus·Sonne·N·L)` + Glow-Map (+ Spekular nur mit `NiSpecularProperty`), siehe §5.
- Ohne SHMD-Umgebung (z. B. reine NIF-Karten): bisherige Editorformeln (`0,35 + 0,65·N·L` bzw.
  mit Vertexfarbe `0,62 + 0,55·N·L`).
- Lichtrichtung mit SHMD-Umgebung waagerecht entlang +X (siehe §5), sonst fest (-0,4, -1, -0,3).
- Nebel: linear vom Auge aus, Start = `Frustum × (1 − Tiefe)`, Ende = `Frustum` (Lesart der Tiefe
  wie Gamebryos `NiFogProperty`-Depth). Im Spieltest immer aktiv, im Editor optional (bei großem
  Kameraabstand würde er sonst die ganze Karte einfärben), in Achsenansichten aus.
- Glow: auf 1/`DownScaling` verkleinern, `NumBlurring` × separierbarer 9-Tap-Gauss
  (σ = 2·`GaussFactor`), Komposition `Szene + Glow · Glowness · BlendFactor`.

Alle drei sind im Viewport-Menü **Anzeigen → Darstellung** einzeln abschaltbar.

## 3. Darstellungsoptionen (Viewport → Anzeigen → Darstellung)

- Ansichtsmodus: Beleuchtet · Unbeleuchtet (nur Textur/Material) · Nur Licht · Normalen ·
  Vertexfarben · UV0 · Alpha · LOD-Stufen (Terrain- und NIF-Shader; Terrain zeigt bei UV die Kachel-UV und ist
  bei Alpha deckend).
- Terrain-Vertexfarben, Kartenlicht (SHMD), Hintergrundfarbe (SHMD), Nebel, Glow,
  Kantenglättung (Aus/2×/4×/8×), Drahtgitter.
- Ansicht: Perspektive oder orthografische Achsenansichten (Oben/Süd/Nord/West/Ost). In Achsen-
  ansichten verschiebt Ziehen, das Rad zoomt; Drehen kehrt zur Perspektive zurück.

## 4. Offene Punkte (ehrlich)

- Abgleich bisher an Rou, RouN und Bera (§5); weitere Karten und Nachtszenen nicht geprüft.
- Lichtrichtung: in keiner Datei enthalten; waagerecht ist gemessen, der Azimut nicht eindeutig.
  `NiFogProperty` in einzelnen NIFs wird weiterhin nur übersprungen (der Kartennebel kommt aus
  der SHMD).
- Bera wirkt im Original heller und türkiser (mittlere Farbe 127/175/151 zu 157/176/156); die
  Ursache (Glow-Formel, Nebel, Vertexfarben der Bodenmodelle) ist offen. Glow mit der jetzigen
  Formel verschlechtert Rou deutlich und bleibt in Aufnahmen aus.
- Die Sternenschicht `starsSmall512` der Himmelskuppel wird auch tagsüber gezeichnet
  (Tag/Nacht-Steuerung des Clients unbekannt).
- Proprietäre Shader, `NiTextureEffect`, `APPLY_HILIGHT2` und Partikelsimulation – siehe
  `docs/LEVEL_EDITOR_PARITY.md` §4.
- Schatten werden nicht berechnet (die gebackenen Vertexfarben enthalten aber Terrain-Schatten).

## 5. Abgleich mit dem Client (NA2016-`resmap`, Screenshots, Portalbilder)

Referenzen: zwei Roumen-Stadtscreenshots (Rou), das Original-Portalbild `TownPortal/Rou.DDS`
(Brunnenplatz), Hafen-/Platz-Screenshots (RouN) und `TownPortal/Bera.DDS`. Nachgestellt mit
`capture <ppm> <B>x<H> <x> <y> <yaw> <pitch> <abstand> [höhe]` (Offscreen, Himmel und Nebel, ohne
Glow); verglichen wurden Struktur (Helligkeit/Kanten) und mittlere Farben je Klasse (Gras, Dach,
Rest). Übernommen wurde nur, was messbar näher am Original liegt.

| Befund | Ursache | Änderung | Beleg |
|---|---|---|---|
| Eingebettete Texturen kopfüber | NiPixelData ist von oben nach unten gespeichert, NIF-UVs folgen Direct3D (v = 0 oben); der Parser spiegelte zusätzlich | keine Spiegelung mehr; externe DDS/TGA/BMP für NIFs werden in dieselbe Lage gedreht | Magiekreis am Rou-Brunnen wird zum Ring wie im Portalbild (vorher „Propeller“), Pferdekopf-Schild steht aufrecht; `test_nif_client_render_state` |
| Graue/weiße Hilfsflächen (RouN-Felsen, Bäume) | NiAVObject-Flag `APP_CULLED` wurde ignoriert | Parts unter einem APP_CULLED-Knoten werden nicht gezeichnet (Mesh-Partikel-Vorlagen bleiben) | `RouN_Rocks.nif`: 2 von 5 Parts, `Urg_AlruinTL01.nif`: 16 Hilfsboxen |
| Weiße Felsen, grelle Lichtstrahlen | Emission wurde ungetextet addiert; `NiVertexColorProperty`-Lichtmodus `LIGHTING_E` (nur Emission) ignoriert | ganze Beleuchtung × Textur; LIGHTING_E = `Textur × Emission` | RouN-Felsen zeigen Textur und gebackene Schatten, Lichtstrahlen dezent (Vertexfarbe ≤ 0,58, Alpha 0,2) |
| Metallisch glänzende Dächer | ohne `NiSpecularProperty` wurde Material-Specular gezeichnet (wie NifSkope) | Gamebryo-Standard: kein Glanz ohne Property | Rou-Dächer matt wie im Screenshot |
| Himmelskuppel als Fläche an der Kartenecke | `field_sky_01.nif` ist um den Ursprung modelliert (±4164, Unterkante +290) | Sky-Kategorie folgt der Kamera | RouN/Rou ohne Himmelsfläche über der Karte |
| Gras/Dächer ~25 % zu hell | Sonnenrichtung steht in keiner Datei (`GlobalLight.nif` der Adl-Karten enthält nur AmbLight-Zonen); ein NiDirectionalLight ohne Rotation leuchtet entlang +X | Lichtrichtung waagerecht (+X): flacher Boden bekommt nur `GlobalLight` | Rou-Stadt: Gras 108/133/49 zu Original 107/139/43 (vorher 135/166/61), Klassenfehler 150 → 66; Bera: mittlere Abweichung 35–38 statt 53–97 |

Nicht übernommen: Glow in Aufnahmen (Rou-Klassenfehler 66 → 166). Die Himmelsrichtung des Lichts
(acht Richtungen getestet) ist zwischen Stadtscreenshot und Portalbild widersprüchlich; +X ist der
Gamebryo-Standard und liegt in beiden nahe am Besten.
