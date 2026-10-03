# Level Editor Production Parity

**Stand:** 26.09.2026  
**Branch:** `ui-upgrade`

Dieses Dokument bewertet NextGen-Editor nicht danach, ob ein Button oder Panel vorhanden ist,
sondern danach, ob der zugrunde liegende Workflow funktional auf dem Niveau eines modernen
Level-Editors belastbar ist.

Referenz sind aktuelle Editor-Workflows aus Unreal Engine 5.8, Unity 6 und Godot 4.x:
orthografische/perspektivische Viewports, geometriebasiertes Picking, Transform-Gizmos,
World/Local, Grid-/Surface-/Vertex-Snapping, editor-only Visibility/Picking, Show Flags,
Marquee-Auswahl und kontextuelle Tool Settings.

Wichtig: Die Referenzeditoren definieren den UX-Maßstab. Fiesta-Dateisemantik wird daraus
**nicht** abgeleitet oder geraten.

---

## 1. Reifegrad-Skala

| Stufe | Bedeutung |
|---|---|
| **A – belastbar** | Workflow und Datenpfad sind für reale Karten praktisch nutzbar und regressionsgesichert. |
| **B – funktional** | Kernworkflow funktioniert, aber wichtige Produktionsdetails/QA fehlen. |
| **C – Prototyp** | Oberfläche und Grundidee existieren, darunter liegen noch Näherungen oder Sonderfälle. |
| **D – Forschung** | Dateisemantik/Runtime-Verhalten ist nicht ausreichend verifiziert. |

---

## 2. Gesamtbild

| Bereich | Stand | Hauptlücke |
|---|---:|---|
| 3D Auswahl / NIF Ray Picking | **A/B** | Material-/Shader-Gleichheit und Spezialeffekte beeinflussen noch die sichtbare Szene. |
| Move / Rotate / Scale Gizmo | **B** | Surface-/Vertex-/Pivot-Snapping, Drag-Duplicate und feinere Multi-Edit-Semantik fehlen. |
| Grid / Angle / Scale Snap | **B** | Kein vollständiges Surface-/Actor-/Vertex-Snap-System. |
| World / Local | **B** | vorhanden; Pivot-/Orientation-Modi noch begrenzt. |
| 2D Objektansicht | **C → B** | bisher konvexe Näherung; wird auf echte Mesh/Boden-Schnittkonturen umgestellt. |
| 2D Objekt-Picking | **C → B** | bisher Radius um Objektursprung; wird an sichtbare Kontaktgeometrie gekoppelt. |
| Box / Lasso | **B/C** | derzeit überwiegend Pivot-/Center-basiert; Crossing/Inside-Semantik auf echte Geometrie fehlt. |
| Eye / Lock | **B** | editor-only Zustand existiert; Isolate/Hide Selected/Show All und per-Viewport Show Flags fehlen. |
| Terrain | **B** | funktional; Brush-Feedback/Presets/Performance und Produktions-QA weiter ausbauen. |
| Texture Paint | **B/C** | reale Layerauflösungen teilweise berücksichtigt; Material-/Layer-Debug und visuelle QA fehlen. |
| Walk / Block | **B** | Preview/Apply/Undo vorhanden; Objekt-Footprint-Stempel nutzt noch konvexe Polygone. |
| NIF Parser | **A strukturell** | strukturelles Laden ist weitgehend verifiziert; visuelle Semantik ist getrennt zu bewerten. |
| NIF Material Rendering | **B/C** | klassische Slots weitgehend da; proprietäre Shader/TextureEffect/Renderstates noch nicht vollständig. |
| NIF visuelle Regression | **C** | echte OpenGL-Smokes existieren, aber kein systematischer Referenzbildvergleich gegen Client/NifSkope. |
| Minimap Editor Preview | **B** | Editor-Preview vorhanden; Fiesta-Exportformat weiterhin absichtlich gesperrt. |
| SHN / Quest / Skill / KFM | **B** | viele reale Workflows vorhanden; je Modul weitere Tiefensemantik/Politur offen. |
| vollständiger Kartenexport | **C/D** | abgeleitete Daten wie IDM können nach Bearbeitung veraltet sein. |

---

## 3. P0 – 2D wird ein echter orthografischer Editor

### 3.1 Boden-Kontaktkontur statt konvexer Näherung

**Alt:** `ComputeFootprintHull()` nimmt die unterste Modellschicht und bildet eine konvexe
Hülle. Dadurch werden konkave Gebäudeformen geschlossen und getrennte Auflageflächen verbunden.

**Neu:** `ComputeGroundContactSegments()` schneidet die echten NIF-Dreiecke mit der lokalen
Boden-/Pivotebene.

Zielregeln:
- authored `y=0` verwenden, wenn die Geometrie diese Ebene schneidet;
- sonst auf die reale tiefste Geometrieebene zurückfallen;
- koplanare Boden-Dreiecke auf Außenkanten reduzieren;
- interne Triangulationskanten entfernen;
- konkave L-/U-Formen erhalten;
- getrennte Standflächen nicht künstlich verbinden;
- keine Bounding Box als sichtbaren „präzisen“ Ersatz ausgeben.

**Status:** Umsetzung begonnen/aktiv auf `ui-upgrade`.

### 3.2 2D Picking entspricht der sichtbaren Geometrie

Ziel:
- Klickdistanz zu den transformierten Boden-Kontaktsegmenten statt großem Radius um den Pivot;
- Pick-Toleranz in Bildschirmpixeln, damit Zoom das Bediengefühl nicht verändert;
- versteckte Objekte sind nicht pickbar;
- gelockte Objekte sind nicht pickbar;
- Pivot-Picking nur als Fallback für Modelle ohne belastbare Kontaktkontur.

Danach:
- Box/Lasso auf Segment-/Projected-Geometry-Intersection statt nur Pivot/Center;
- Inside-vs-Crossing-Marquee als explizite Selection-Option.

### 3.3 Orthographic View Modes

Nach der Kontaktkontur:
- Top als echter orthografischer Viewport-Modus behandeln;
- Front / Side ergänzen, sobald Transform-/Picking-Code dieselbe Projektionsabstraktion nutzt;
- Wireframe / Contact / Pivot / Gameplay Helper als getrennte Show Flags;
- Viewport-Helfer dürfen Editorzustand ändern, niemals Fiesta-Runtime-Daten.

---

## 4. P0 – NIF Material Fidelity

Der Parser und der Renderer müssen getrennt bewertet werden.

### 4.1 Bereits vorhanden

Der aktuelle NIF-Datenpfad erhält bzw. rendert bereits:
- Base / Dark / Detail / Gloss / Glow / Bump / Decal0..3;
- bis zu acht UV-Sets;
- Clamp-/Filter-Flags je Slot;
- Texture Transforms;
- externe und eingebettete `NiPixelData`;
- Texture Transform Controller / Flip Controller;
- Material Ambient / Diffuse / Specular / Emissive / Glossiness / Alpha;
- Alpha Test und Blend-Selektoren;
- FaceDrawMode / Culling;
- LOD / Billboard-Grundpfad;
- `VCAlphaTextureBlender` als expliziten Spezialshader;
- DDS/TGA sowie unter Windows JPG/JPEG/PNG/BMP.

Das erklärt, warum „fehlende Textur“ nicht mehr pauschal mit einem fehlenden Dateipfad
gleichgesetzt werden darf.

### 4.2 Noch offene visuelle Semantik

Priorisiert nach realem Asset-Korpus untersuchen:
1. weitere tatsächlich vorkommende `shaderName`-Familien;
2. `NiTextureEffect` – strukturell gelesen, Rendersemantik noch nicht materialisiert;
3. `NiVertexColorProperty` – Property wird gelesen/übersprungen, Renderstate-Semantik gesondert prüfen;
4. `NiZBufferProperty` – Depth-Funktionen/-Writes aus echten Dateien inventarisieren;
5. proprietäre `NPTR_ISShader_v2` / Toon-ExtraData und verwandte Fiesta-Pfade;
6. Partikel-/Effect-NIFs getrennt von statischen Map-Meshes behandeln.

### 4.3 Diagnose statt „Textur fehlt“

NIF Inspector soll pro Mesh-Part unterscheiden:
- **OK: external texture resolved**
- **OK: embedded PixelData decoded**
- **missing external asset**
- **embedded PixelData unsupported/invalid**
- **texture slot present but UV set invalid → UV0 fallback**
- **known shader path**
- **shader/effect semantics unsupported**
- **partial/recovery model**

Damit kann ein sichtbarer Renderfehler einer Ursache zugeordnet werden, statt alles als
„missing texture“ zu melden.

### 4.4 Visuelle Regression

Eine feste Referenzsuite aus echten Fiesta-NIFs aufbauen:
- einfache opaque BaseMap;
- Alpha-Test Vegetation;
- transparente Flächen;
- Detail/Gloss/Glow;
- Bump;
- Decal;
- VCAlphaTextureBlender;
- Billboard;
- LOD;
- eingebettete PixelData;
- externe DDS/TGA/JPG;
- je gefundener proprietärer Shaderfamilie mindestens ein Modell.

Pro Fixture:
- Parserdiagnose;
- Materialslot-Dump;
- OpenGL-Render;
- Referenzbild/visuelle Prüfung gegen NifSkope bzw. belegte Clientdarstellung;
- Golden-Image nur dort automatisieren, wo GPU-/Treiberunterschiede robust tolerierbar sind.

---

## 5. P1 – Transform und Snapping auf Produktionsniveau

Bereits vorhanden:
- Move / Rotate / Scale;
- World / Local;
- Move-/Rotate-/Scale-Inkremente;
- Drop to Terrain;
- Focus Selection;
- Multi-Selection.

Noch nötig:
- Surface Snap während des Ziehens;
- optional Rotate to Surface Normal;
- Vertex Snap;
- Actor/Pivot Snap;
- Pivot bearbeiten / Center-vs-Pivot-Modus;
- Duplicate-while-dragging;
- Multi-Transform Inspector mit Mixed Values;
- Transformänderungen als ein sauberer Undo-Schritt pro Drag.

---

## 6. P1 – Editor Visibility / Picking / Isolation

NextGen trennt Editor-Hide/Lock bereits von SHMD-Exportdaten. Das ist die richtige Grundlage.

Ergänzen:
- Hide Selected;
- Hide Unselected;
- Isolate Selected;
- Show All;
- globaler „Scene Visibility enabled“-Schalter;
- getrennte Pickability und Visibility;
- per-Viewport Show Flags;
- Status/Count versteckter Objekte;
- persistente Editor-Sichtbarkeit ohne Änderung der Spiel-/Exportsemantik.

---

## 7. P1 – Selection Semantics

### Einzelklick
- 3D: echte Dreiecke (bereits weitgehend umgesetzt).
- 2D: sichtbare Kontakt-/Projektionsgeometrie.

### Marquee
- **Inside**: Objekt vollständig im Rahmen.
- **Crossing**: jede geometrische Überschneidung zählt.
- Add / Remove / Replace eindeutig über Modifier.

### Lasso
- gleiche Semantik wie Marquee;
- nicht nur Objektpivot testen.

### Transparenz
Eine spätere Viewport-Option entscheidet, ob transparente NIF-Flächen pickbar sind.
Die Einstellung muss rein editorseitig sein.

---

## 8. P2 – Viewport Debug / Show Flags

Produktive Material-/Levelarbeit braucht Diagnoseansichten:
- Lit;
- Unlit;
- Wireframe;
- Base Color;
- UV0..UV7;
- Vertex Color;
- Normals;
- Alpha;
- Material Slot Occupancy;
- Ground Contact;
- Collision / Walk;
- NPC/Mob/Portal Helpers;
- LOD Level.

Diese Modi sind Debug-/Editoransichten und verändern keine Fiesta-Datei.

---

## 9. P2 – Scene Productivity

Nach Geometrie-/Materialkorrektheit:
- Align X/Y/Z;
- Distribute;
- Match Transform;
- Replace Selected with Asset;
- Duplicate Pattern / Array;
- Prefab-/Template-Gruppen als Editor-Metadaten;
- Auswahlsets;
- Bookmarks / gespeicherte Kamerapositionen.

---

## 10. P0/P1 – Fiesta Export Safety

Laut aktuellem Formatstatus können abgeleitete Begleitdaten nach Änderungen veraltet sein.

Deshalb:
- Dirty-Derived-Data-Status explizit anzeigen;
- Exportwarnung, wenn IDM/andere abhängige Daten nicht neu berechnet werden können;
- niemals „vollständig spielbereit“ behaupten, solange die Neuberechnung nicht verifiziert ist;
- Minimap-Export weiterhin gesperrt lassen, bis das echte Format belegt ist.

---

## 11. Umsetzungskette ab 26.09.2026

1. **Exakte NIF-Boden-Kontaktsegmente** + synthetische konkave Regression.
2. **2D-Kontur-Rendering** ohne Origin-Marker bei belastbarer Geometrie.
3. **2D-Kontur-Picking**, Hidden/Lock respektieren.
4. Box/Lasso auf projizierte Geometrie umstellen.
5. Reale NIF-Archive nach Shader-/Effect-Familien inventarisieren.
6. NIF Inspector um Material-/Shader-Diagnose erweitern.
7. Wichtigste reale Shader-/Renderstate-Lücken implementieren.
8. Referenz-NIF-Suite + visuelle Regression.
9. Surface-/Vertex-/Actor-Snapping.
10. Isolate/Hide Selected/Show All + per-Viewport Show Flags.
11. Orthographic Front/Side und Debug-View-Modes.
12. Derived-Data-/Export-Sicherheit.

---

## 12. Externe UX-Referenzen

Geprüft am 26.09.2026:

- Epic Games, **Unreal Engine 5.8 – Viewport Toolbar / Viewport Controls / Quick Settings**:
  Transform & Snapping, World/Local, Surface/Vertex/Actor snapping, Perspective/Orthographic,
  Marquee Selection und View/Show Flags.
- Unity Technologies, **Unity 6 – Scene View / Grid and Snap / Scene Visibility**:
  Transform-/Grid-Overlays, Orthographic axis views, editor-only Visibility, Isolation und
  getrennte Pickability.
- Godot Engine, **Godot 4.x – Introduction to 3D**:
  Local Space, Snap Settings, Snap Object to Floor sowie Grid-/Gizmo-Visibility.

Diese Referenzen definieren nur die Editor-Interaktionsqualität. Fiesta-spezifische
Datei-/Runtime-Semantik wird ausschließlich aus verifizierten Fiesta-Daten abgeleitet.
