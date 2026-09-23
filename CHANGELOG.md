## v0.44.35 / v12 — struktureller NIF-Standardparser, Partikel und Vollbestandstest
- `NiGeometryData`-Group-ID vor der Vertex-Anzahl; versionsabhängige ParticleDesc-, Rotations- und Emitterfelder.
- `NiMeshPSysData` liest den variablen Bereich `13 + 4*N` statt pauschal 17 Bytes. Zusätzliche Particle-Controller, Zylinder-/Kugelemitter und Bomb-/Kollisionsmodifikatoren.
- Dokumentierte Material-, Geometry-, SourceTexture-, PixelData-, Palette- und LOD-Felder werden im Standardpfad exakt abgegrenzt. Recovery behält den alten Kompatibilitätspfad, explizit markiert als `recovered`/`partial`.
- Block-Marker von NIF 10.1.0.0 werden vor jedem Block gelesen; TextureEffect-PS2-Felder für 10.x ergänzt. NiCamera, NiShadeProperty und dokumentierte Controller-/B-Spline-Strukturen ergänzt.
- Exakte Footer-Prüfung; regressionsgesicherte Partikel-Arraylängen, Trunkierung, Material-/SourceTexture-Grenzen und echte Fixture-Modelle.
- Prozessisolierter Massentest mit hartem Timeout, fortlaufendem TSV, Archivstatus, SHA-256-Inventar und getrennten Standard-/Recovery-Ergebnissen.
- Vollständiger Windows-Core-/GUI-Build; 13/13 CTest; echter OpenGL-Smoke-Test mit fünf Modellen. Ergebnisse und verbleibende Einschränkungen: `docs/V12_VALIDATION.md`.

## NIF rendering follow-up — texture animation & paletted embedded textures v11
- Restore the project fixture corpus under `tests/fixtures`; the complete headless CTest suite now passes 10/10 again.
- Parse and retain `NiTextureTransformController`, `NiFlipController`, `NiFloatInterpolator` and `NiFloatData` controller chains attached to `NiTexturingProperty`.
- Evaluate texture-controller timing at draw time, including frequency/phase, cycle/reverse/clamp extrapolation and linear/quadratic/constant float keys.
- Animate per-slot U/V translation, rotation and U/V scale; resolve NiFlipController source lists into runtime flipbook frames (external or embedded textures).
- Validate controller linking on a 20-file real NIF corpus: multiple rendered flipbooks resolve with 10–32 frame source lists while texture-transform tracks remain attached to their target slots.
- Decode Fiesta's paletted embedded-texture variant structurally (`Bytes Per Pixel == 1` plus a `NiPalette` reference) instead of relying only on the nominal PixelFormat enum; defer expansion when the palette block occurs after `NiPixelData`.
- `filddoll.nif` now resolves its deferred palette and loads with 9 embedded textures. The older `machine.nif` block-67 `NiPixelData` alignment failure remains unchanged and is tracked separately rather than being hidden by the palette fix.
- `mapeditor_core`, `test_nif_model`, and the standalone `NifMeshRenderer.cpp` syntax check pass after the changes. No real GUI/OpenGL runtime session was performed in this environment.

## NIF rendering follow-up — skinning v9
- Parse `NiSkinInstance`, `NiSkinData` and `NiSkinPartition` instead of only skipping them.
- Preserve geometry-to-skin-instance links and evaluate weighted CPU skinning in the current skeleton/bind pose.
- Support both partition weights (`Vertex Map`, `Bone Map`, per-vertex weights/indices) and legacy sparse weights from `NiSkinData`.
- Transform normals with the same weighted bone rotations and renormalize them after skinning.
- Keep skin diagnostics per mesh part (`skinned`, bone count, maximum influences).
- Regression corpus: 46/47 unique skinned `resmap` NIFs load successfully; the one failure (`machine.nif`, `NiPixelData`) already fails identically in v8, so no new load regression was found in this corpus.
- Merge the GUI build fallback that pins GLFW 3.5.1 through CMake `FetchContent` when no installed `glfw` target exists.

# Changelog — TheSeed Map-Editor

## NIF rendering follow-up — runtime NiBillboardNode & NiLODNode
- Preserve NiBillboardNode mode, pivot and inverse baked scene rotation per mesh part instead of treating billboards as ordinary static nodes.
- Apply camera-facing billboard orientation at draw time; ROTATE_ABOUT_UP-style modes keep the world up axis while the remaining modes follow NifSkope's camera-facing behavior.
- Preserve NiRangeLODData near/far ranges and associate each direct NiLODNode child with its range.
- Select LOD geometry per placed object at runtime using the camera-space distance of the LOD center (`near <= distance < far`), matching NifSkope's selection rule.
- Stop permanently discarding non-selected LOD children during NIF loading; all levels remain available for runtime switching.
- Headless `mapeditor_core` build verified; `NifMeshRenderer.cpp` syntax-checked against an OpenGL stub. CTest remains 8/10 because the package still lacks `tests/fixtures/Rou.ini` for the same two legacy-texture tests.

## NIF rendering follow-up — transparent passes, FaceDrawMode & sampler state
- Render opaque/alpha-test geometry first and blended submeshes in a second back-to-front pass.
- Sort transparent submeshes by transformed local AABB center in camera space while keeping depth testing enabled and depth writes disabled.
- Correct NiStencilProperty FaceDrawMode semantics: 1=DRAW_CCW, 2=DRAW_CW, 3=DRAW_BOTH; mode 0/unknown remains double-sided in the editor to avoid hiding geometry.
- Change the default FaceDrawMode to DRAW_BOTH when no stencil property is present.
- Apply NiTexturingProperty clamp and filter modes to the bound diffuse texture, including nearest/bilinear/trilinear mipmap variants.
- Cache shader uniform locations per draw call and restore the OpenGL program/VAO/texture/depth/blend/cull states after NIF rendering.

## [0.44.35] — NIF-Untersuchung fortgesetzt: ein weiterer Bitmasken-Fehler behoben, 184 Dateien bleiben offen

Auf Bitte, die verbleibenden ~191 Karten-NIFs zu lösen: systematisch nach Vorgänger-Blocktyp gruppiert (welcher Block
geht dem jeweiligen Fehlschlag unmittelbar voraus) und die größten sauberen Gruppen einzeln gegen mehrere autoritative
Referenzen (nif.xml-Community-Dokumentation, die "nif" Rust-Crate von docs.rs - binrw-generiert direkt aus nif.xml,
Ziel-Version exakt 20.0.0.4) geprüft.

**Gefunden und behoben:** `SkipNiGeometryDataHeader`/`ParseNiTriStripsData`/`ParseNiTriShapeData` prüften die
Tangentenraum-Bedingung mit der Maske `0x1000` (nur Bit 4 von `tspace_flag`) statt `0xF000` (alle 4 oberen Bit, laut
Referenz `tspace_flag & 240`). Bei einer Datei mit z.B. 0x20/0x40/0x80 statt 0x10 gesetzt würden Tangenten+Binormalen
(2×numVerts×12 Byte) fälschlich NICHT gelesen - eine datenproportionale Verschiebung, die keine Verschiebungssuche
findet. Vollvergleich über alle 3433 Karten-NIFs: **0 gewonnen, 0 verloren, 0 mit anderer Dreieckszahl** - im aktuellen
Korpus tritt der Fall nirgends auf, die Korrektur ist aber durch die Referenz belegt und schützt vor künftigen/anderen
Dateien mit diesem Bitmuster. Behalten, da risikofrei und nachweislich korrekt.

**Mehrere Gruppen einzeln untersucht, KEIN Fix gefunden (Struktur stimmt bereits mit der Referenz überein):**
- `NiPSysBoxEmitter`/`NiPSysVolumeEmitter`/`NiPSysEmitterBase`/`NiPSysModifierBase` (Kette vor 9 `NiPSysSpawnModifier`-
  Fehlschlägen, z.B. `FirePath.nif`): Byte-Länge (85 Byte bei leerem Namen) trifft exakt die Referenzstruktur.
- `NiParticlesData`/`NiPSysData` (Partikel-Kopfstruktur): Feldreihenfolge und -größen stimmen exakt.
- `NiCollisionData`/`NiCollisionObject` (Kette vor 11 `NiTriStripsData`-Fehlschlägen, z.B. `floor_1.nif`, `ship.nif`):
  Struktur stimmt; die tatsächliche Fehlerursache liegt vermutlich in einem noch nicht identifizierten Feld eines
  VORANGEHENDEN `NiTriStrips`-Blocks in dieser spezifischen Kette (zwei komplette NiTriStrips+NiCollisionData-Teilbäume
  hintereinander - der erste parst korrekt, der zweite nicht).
- `NiAvObject`/`ParseAVObjectBase`: Struktur stimmt exakt (flags, translation, rotation, scale, properties, collision_ref).

**Bewusst NICHT angefasst (zu hohes Risiko ohne deutlich mehr Prüfzeit):** `ParseObjectNetBase`s Peek-Heuristik für ein
eventuell fehlendes `num_extra_data_refs`-Feld. Die autoritative Referenz kennt hier KEINE Bedingung (das Feld ist
immer vorhanden) - unser Code weicht davon ab, weil mehrere echte Testdateien in früheren Sitzungen mit der
unbedingten Variante nachweislich falsch lagen (siehe Kommentare im Code, u.a. `AdlFH_field_burn_ground.nif`). Diese
Funktion wird von praktisch jedem Blocktyp verwendet - eine Änderung ohne vollständige Verifikation hätte das Potenzial,
die bestehenden 94.6% zu gefährden, nicht nur die 184 offenen Dateien zu verbessern. Nicht spekulativ geändert.

**Status unverändert: 94.6% (3249 von 3433 Karten-NIFs), 184 offen.** Größte verbleibende Gruppen unverändert seit
[0.44.34]: `NiNode` nach Partikelsystem-Modifikator-Ketten (~61, Ursache nicht in den einzeln geprüften Modifier-Typen
selbst), `NiBillboardNode` nach `NiTriStrips` (16) bzw. nach `NiPSysMeshUpdateModifier` (7), `NiTriStrips` nach
diversen Vorgängern (kein einzelner dominanter Grund), `NiPixelData` nach `NiSourceTexture` (15, wahrscheinlich in
`NiTexturingProperty`s vielfach bereits revidierter "Num Shader Textures"-Bedingung, siehe HANDOFF Nr. 24/25).

**Nicht geprüft:** MSVC-Build; ob die offenen Dateien tatsächlich Gebäude oder überwiegend Effekte/Deko sind, wurde in
[0.44.34] bereits grob nach Namen geschätzt (~40 von damals 192 mit gebäudeartigem Namen).

## [0.44.34] — NPC-Ausrichtung korrigiert, Assert-Absturz behoben, weitere Objekt-NIFs

**NPC-Blickrichtung war falsch (Nutzermeldung).** Statt weiter zu raten: datengestützte Analyse über 28 Karten mit
vollständigem Block&Walk-Gitter und 270 NPCs - für jede Kombination aus Vorzeichen und Versatz geprüft, wie oft ein NPC
"vorne" auf begehbarem und "hinten" auf blockiertem Gelände steht (Annahme: NPCs stehen mit dem Rücken zur Wand - plausibel,
nicht bewiesen). Ergebnis eindeutig: das in v0.44.32 ausgelieferte Vorzeichen (`sign=-1`) war über den GESAMTEN
Wertebereich durchgehend schlechter als `sign=+1`; bester Versatz empirisch bei ca. -20°, Standard jetzt auf den
einfacheren, nicht überangepassten Wert 0° gesetzt (`sign=+1, offset=0`). Der Versatz-Regler ist jetzt ein freier Schieber
(-180° bis 180°, vorher vier feste 90°-Stufen), dazu ein neuer Knopf **"Versatz aus dieser Karte schätzen"** im NPC-Tab, der
dieselbe Analyse live für die offene Karte wiederholt. Test `test_npc_orientation` (bindet main.cpp direkt ein, neu im
CTest-Set) verankert das Vorzeichen als Regressionsschutz.

**"ENABLE ASSERT am Mauszeiger" (Nutzermeldung) - Ursache: NaN-Absturz in Heightmap::At.** `Heightmap::At`/`Set` haben ein
`assert()` gegen ungültige Koordinaten. `SampleWorld()` (läuft bei jeder Mausbewegung) teilt durch die Blockgröße der Karte;
fehlt/verunglückt `OneBlockWidth`/`OneBlockHeight` in der .ini, lieferte `ParseFloatSafe` dafür 0.0 zurück. Bei Weltkoordinate 0
ergibt das `0/0 = NaN`, und NaN nach `uint32_t` zu casten ist undefiniertes Verhalten - reproduzierbar bei jeder
Mausbewegung nahe Weltursprung/Kartenrand. Drei Verteidigungslinien: der .ini-Parser fällt bei fehlendem/ungültigem Wert auf
50.0 zurück statt auf 0.0; `Heightmap::SetBlockSize` (auch im Konstruktor) ignoriert 0/negative/NaN-Werte und behält den
vorherigen gültigen Wert; `SampleWorld` fängt NaN-Weltkoordinaten zusätzlich selbst ab. Neue Tests in
`test_heightmap_core` (Konstruktor, SetBlockSize, SampleWorld, .ini-Fallback).

**Weitere Objekt-NIFs (Fortsetzung [0.44.31]):** `NiStencilProperty` hatte ein festes, bedingungslos gelesenes
Namens-/Beschreibungsfeld am Ende - laut autoritativer nif.xml-Struktur (verifiziert über die "nif" Rust-Crate-
Dokumentation, binrw-generiert) gehört dort KEIN Sized-String hin. Das Feld wird jetzt nur noch gelesen, wenn die
nächsten Bytes wie ein plausibles, nicht leeres Textfeld aussehen UND nicht schon der Name des nächsten Blocks sind;
bei einer weder leeren noch plausiblen Länge scheitert der Block jetzt sauber (statt mit falscher Position
weiterzulesen) und gibt der Verschiebungssuche einen Ansatzpunkt. Vollvergleich über alle 3433 Karten-NIFs: 3241 -> 3249
ladbar (94.4% -> 94.6%), 9 Dateien neu ladbar (`stadium.nif`, `Urg_lefte_w_snow_blend.nif` (2x verschiedene Instanzen),
`swa_leaf01.nif`, u.a.), **1 Datei verloren** (`UrgFire01/ElderinGround.nif` - die alte, bedingungslose Variante
interagierte an dieser Stelle zufällig günstiger mit der Verschiebungssuche; unter dem Strich klar positiv, aber bewusst
nicht verschwiegen). `swa_leaf.nif` lädt jetzt mit mehr Dreiecken (3726 -> 4032, vermutlich vollständigere Geometrie).
Mehrere weitere Ansätze (Suchbudget/-zeitlimit weiter erhöht, Namens-Resync-Reichweite von 16 auf 96 Byte) blieben ohne
Wirkung auf die verbleibenden Fehlschläge und wurden verworfen (keine Codeänderung).

**Noch offen, NICHT "vollständig" (191 von 3433 Karten-NIFs, davon rund 40 mit Gebäude-artigem Namen wie `Gamble_House.nif`,
`GATEROOM.nif`, `BINROOM.nif`, `Tower.nif`, `MapLinkGate.nif`):** die größte verbleibende Gruppe (NiNode-Fehlschlag direkt
nach einer Kette von Partikelsystem-Modifikatoren, ca. 30 Dateien) ließ sich nicht auf einen einzelnen fehlerhaften
Blocktyp zurückführen - `NiPSysRotationModifier`, `NiPSysPositionModifier`, `NiPSysBoundUpdateModifier` und
`NiPSysMeshUpdateModifier` wurden einzeln gegen die autoritative Struktur geprüft und sind korrekt; die Ursache der
Byte-Verschiebung liegt vermutlich in `NiParticleSystem`/`NiPSysData` selbst (nicht weiter untersucht - hoher Aufwand,
unklarer Erfolg). `Gamble_House.nif`/`GATEROOM.nif`/`stadium.nif`(-Duplikate) haben je EIGENE, unterschiedliche
Fehlerursachen (NiTriStrips->NiNode bzw. NiCollisionData->NiTriStripsData) - kein gemeinsamer Fix gefunden.

**Nicht geprüft:** NPC-Blickrichtung und Assert-Fix im echten Fenster/Spiel, die neu ladbaren NIFs optisch, MSVC-Build.

## [0.44.33] — Handbuch, Tooltips, Skill-Editor, Tastenkürzel

**Integriertes Handbuch (F1 oder Knopf "? Handbuch" unten rechts, auf JEDEM Bildschirm):** 25 Abschnitte in 6 Kapiteln (Erste
Schritte, Steuerung und Tastenkürzel, Map-Editor je Tab, SHN-Editor, Erstellen von NPC/Mob/Skill/Shop, Nachschlagen), Volltextsuche
(alle Wörter, Titeltreffer zuerst), komplett Deutsch und Englisch (Sprachumschalter DE/EN in der Leiste; die englischen Texte sind
nicht von einem Muttersprachler geprüft). Dazu die **Spalten-Referenz (live)**: alle in den SHN-Editor geladenen Tabellen mit Spaltennummer,
Typ und - wo bekannt - Beschreibung (195 dokumentierte Spalten; Abgeleitetes ist als "(vermutet)" markiert, Unbeschriebenes zeigt "-").

**Tooltips für JEDES Bedienelement:** Neue Hülle `UI::Button/Checkbox/SliderFloat/InputInt/InputText/Combo/...` (340 Aufrufstellen umgestellt)
zeigt nach kurzer Verzögerung den Tooltip aus dem Datenbestand (265 Einträge, DE/EN); Beschriftungen aus `T("...")` werden über die
Übersetzungstabelle aufgelöst, gleiche Beschriftung in verschiedenen Widget-Arten (`X`) hat je Art einen eigenen Text. Der Test
`test_manual <main.cpp>` prüft, dass jedes Bedienelement mit Text-Literal im Code einen Tooltip in beiden Sprachen hat (256 geprüft).
Pflege: `docs/MANUAL_MAINTENANCE.md` - Daten in `tools/manual/*.py`, Generator `tools/manual/gen.py` -> `src/core/ManualData.cpp`.

**Tastenkürzel (neu):** F1 = Handbuch; **Strg+Z / Strg+Y (Strg+Shift+Z)** = Rückgängig/Wiederholen im aktiven Werkzeug (Höhe, Textur,
Block&Walk) - die Beschriftungen "(Strg+Z)" gab es schon, die Tastenkürzel selbst fehlten bisher.

**Skill-Editor** (SHN-Editor, 8. Tab): Skills ändern UND neue aus vorhandenen Animationen/Effekten erstellen.
- Ein Skill = eine Stufe einer Reihe (TripleHit14). Daten in `ActiveSkill` (Client+Server), `ActiveSkillInfoServer`, `ActiveSkillView` (Client+
  Server/View); lernbar über ein Skillbuch-Item mit gleichem InxName (ItemInfo/ItemInfoServer/ItemViewInfo).
- Liste mit Suche (Name/InxName/ID), Formular in Bereichen (Grunddaten, Kosten und Zeiten, Schaden, Bewegung/Ziele, Server-Werte, Darstellung,
  Zustände A-D, alle übrigen Spalten roh), jede Änderung sofort in ALLE Kopien (Client und Server). Tooltips je Feld ("(vermutet)" wo nur
  aus dem Namen abgeleitet).
- Animationen (Zauber-Bereitschaft/Zaubern/Ausführung), Effekte (Geschoss/Treffer/Fläche/Dauerschaden), Sounds und Icon sind NAMEN: "Auswahl..." listet
  die in den Daten verwendeten Werte nach Häufigkeit (z.B. 799 verschiedene Ausführungs-Animationen, 56 Geschoss-Effekte) - so lassen sich neue
  Skills aus vorhandenen Animationen und Effekten zusammenstellen. Neue Animations-/Effekt-DATEIEN kann der Editor nicht erzeugen.
- "Neuen Skill aus diesem anlegen": klont alle 5 Tabellenzeilen (freie ID über alle Tabellen, InxName eindeutig geprüft), optional mit Skillbuch-
  Item (eigene freie Item-ID); "Nächste Stufe anlegen" (Stufe+1, Voraussetzung = Vorstufe); "Ganze Reihe skalieren" (Schaden/Kosten/Abklingzeit/
  Zauberzeit in % über alle Stufen, Client+Server).
- Test (Headless, `ph15`): Änderung landet in Client- und Server-Kopien, Klon in 5 Tabellen + Skillbuch in 5 Item-Tabellen, Serie skaliert (80 Werte),
  Speichern und Neuladen (Server-ActiveSkill 2792 Zeilen).

**Nicht geprüft:** Ob das Spiel selbst erstellte Skills akzeptiert (Client-Cache, Skill-Baum, Server-Prüfungen); die Bedeutung der als "(vermutet)"
markierten Spalten; F1/Strg+Z im echten Fenster; Inhalt und Übersetzung der Handbuchtexte durch den Nutzer; MSVC-Build.

## [0.44.32] — Kamera (Ego-Steuerung), 2D-Zoom, NPCs im 3D-Editor

**3D-Kamera - "in das eintreten, was wir bauen":** rechte Maustaste halten + Maus = umsehen (Ego-Kamera: die
Augenposition bleibt stehen, das Ziel wandert mit - `OrbitCamera::LookBy`), **W/A/S/D** (oder Pfeiltasten) = laufen,
**Q/E** (oder Leertaste) = runter/hoch, **Shift** = 4x schneller, **Strg** = langsam, **Mausrad** = Zoom (multiplikativ,
12 % je Schritt, bis auf 1.5 Einheiten an den Punkt), mittlere Taste = schieben, linke Taste ziehen = um das Ziel kreisen (wie
bisher). Lauftempo wächst mit der Entfernung (150..6000 Einheiten/s). Nahe Clip-Ebene dynamisch (0.4..15 statt fest 10 -
nah dran wurde vorher alles abgeschnitten), ferne 90000. Nahe am Ziel darf die Kamera nach oben schauen (negativer Pitch),
aus großer Entfernung bleibt sie über dem Boden. Kurzhilfe unten links im Bild. Tests in `test_camera_handedness`
(Augenposition bleibt beim Umsehen, Laufen = exakt N Einheiten, Zoom, Clip-Ebenen).

**2D-Zoom:** Mausrad zoomt um den Mauszeiger (1x..40x, der Kartenpunkt unter dem Zeiger bleibt), mittlere/rechte Taste ziehen =
verschieben, Knöpfe + / - / 1:1 oben rechts, Zoom-Anzeige unten links. Umsetzung: `imageSize`/`cursorScreenPos` beschreiben
ab jetzt das VIRTUELLE, gezoomte Kartenbild - alle Marker-/Maus-/Pinselumrechnungen gelten unverändert; der Renderer zeigt
per `SetTopDownWindow` nur den Ausschnitt, Marker werden auf den Viewport beschnitten, das Block&Walk-Overlay bekommt
UV-Offset/-Skala. Auswahl-Toleranz skaliert mit dem Zoom.

**NPCs im 3D-Editor:**
- Modellsuche jetzt in ALLEN Client-Ordnern `res*` (außer ressystem/resmenu/resmap/resitem): zuerst `reschar/<Name>/<Name>.nif`,
  dann jeder weitere Ordner als `<Ordner>/<Name>/<Name>.nif` bzw. `<Ordner>/<Name>.nif` (Schreibweise egal), zuletzt die NIF-
  Bibliothek von resmap. NPC-Modelle im Client-`reschar` (nicht in `reschar.zip` der Spielerklassen) laden mit dem
  verbesserten NIF-Parser (BoneLODController, Shader-Feld, Part-Neuaufbau).
- Charakter-NIFs werden für die Anzeige aufbereitet (`SimplifyCharacterModel`): unsichtbare Knochen-Hüllen und die
  Detailstufen-Duplikate entfallen (Fighter-m: 23 -> 3 Teile). Texturen neben dem Modell werden schreibweisenunabhängig gefunden.
- **Spieler-Avatar-NPCs** (`MobViewInfo.NpcViewIndex != 0`, z.B. HednisFigGuard01) werden aus `NPCViewInfo.shn` (Klasse,
  Geschlecht, Gesicht, Frisur, Ausrüstung) mit dem Avatar-Baukasten zusammengesetzt und als Modell gezeichnet
  (`AvatarToNifModel`, `NifMeshRenderer::LoadModelsForSet(..., custom)`). Test EldGbl02: 5 Avatar-NPCs gebaut.
- **Ausrichtung:** Blickpfeil + Name jedes NPCs im 3D-View (NPC-Modus) und Blicklinie im 2D-View; im NPC-Tab Richtung
  -15/+15/+90/180, "Kamera zu diesem NPC", und die Zuordnung Richtung -> Blickwinkel ist einstellbar (Drehsinn +/-,
  Versatz 0/90/180/270 Grad; Standard: gespiegelt, 180). Winkel = Vorzeichen * Richtung + Versatz, Modell schaut bei 0 nach
  -Z. **Diese Zuordnung ist NICHT aus Daten belegt** (kein Ground-Truth für "wohin schaut ein NPC") - bitte an einem bekannten
  NPC prüfen und ggf. umstellen. Coord-/Richtung-Eingaben positionieren die Modelle nur neu (`RefreshNpcTransforms`), statt
  alle Modelle neu von der Platte zu laden. Karten-NPCs ohne auffindbares Modell werden im Bereich "Sichtbarkeit" namentlich gelistet.

**Tests:** alle Suiten grün; `test_avatar_preview` prüft Aufbereitung/Konvertierung; Headless: NPC-Auflösung (fremder `res*`-Ordner
mit anderer Schreibweise, Avatar-NPCs, Richtung -> Winkel). NICHT geprüft: 2D-Zoom und 3D-Kamera im echten Fenster (GL), die
NPC-Modelle deines Client-`reschar` (in meinen Daten nur Spielerklassen), Modellgrößen/Höhe der NPCs auf dem Gelände, MSVC-Build.

## [0.44.31] — Restliche Objekt-NIFs (Karten-Modelle): Ladbarkeit 87.8 % -> 94.4 %

Grundlage: ALLE NIFs aller `resmap`-Zips (3429 Dateien, nicht nur die kleine Stichprobe) - dabei zeigte sich, dass die
Fehlerquote deutlich höher war als bisher gemeldet (12.2 % statt 3.7 %). Vollvergleich Vorgängerstand (v0.44.30) gegen
diesen Stand: geladen 3010 -> 3237 von 3429, **0 Dateien verloren**, Gesamtlaufzeit 1222 s -> 347 s. Karten-Objekte mit
Modell: Rou 1564 -> 1572 von 1580 (Rest: leere Modelle), Adl 1297 von 1321 (24 Dateien fehlen in den Zips), Cypian 2295 von
2384 (89 fehlen), 0 "NIF nicht ladbar".

**Ursachen und Korrekturen:**
- **u16-Grenzen zu niedrig:** Vertex-Anzahl (u16) und Streifenlänge (u16) wurden ab 20000 bzw. 60000 abgelehnt - große
  Mauern/Türme haben z.B. 24136 Vertices und einen Streifen mit 61302 Indizes (`vannel_city_wall.nif`, jetzt 127886
  Vertices). Grenzen jetzt 65535; zusätzlich eine billige Plausibilität gegen die restliche Dateigröße (Vertices*12,
  Streifen*2) - ohne sie wurden falsch ausgerichtete Probeläufe der Suche 8x langsamer.
- **`keep_flags` ist ein Bitfeld:** neben 0/1 kommt 0x33 vor (`adl_town_teras_ground.nif`, `bail80_DG.nif`);
  `LooksLikeTriDataHeader` lehnte den Blockanfang deshalb ab. Jetzt müssen nur `compress_flags` und `has_vertices` 0/1 sein.
  Allein das rettete 142 Dateien (3072 -> 3214).
- **NiTriStrips/NiTriShape-Header, `has_shader=0`:** die folgenden 4 Byte sind in vielen Dateien ein ECHTER String
  (Länge > 0), meist der leere String; steht dort 0xFFFFFFFF ("Active Material" = -1, z.B. `Urg_AlruinTW.nif`), ist es ein
  i32. Nur dieser Fall wird gesondert gelesen. Bei `has_shader=1` folgt nach dem Namen ein i32 NUR bei nicht leerem Namen.
  (Ein Versuch, die 4 Byte immer als i32 zu lesen, verlor 638 vorher ladbare Dateien - zurückgenommen.)
- **Tolerierte Endblöcke** (Teilmodell, wenn nur dort das Parsen scheitert): zusätzlich `NiMorph*`, `NiGeomMorpher*`,
  `NiRotatingParticles`, `NiUVData`, `NiRollController` (z.B. `Urg_AlruinTW.nif`, `S_Tower01.nif`, `Wedding.nif`).
- **Suche (Varianten/Verschiebungen):** Probe-Budget nach Dateigröße, EIGENES Budget je Stufe (die Material-x-Pixel-Varianten
  fraßen sonst das der Verschiebungssuche), Zeitlimit 6 s je Datei, Tiefensuche wie im Original, Verschiebungen jetzt
  ±4/±8/±12/±16 Byte.
- **Namens-Resynchronisation** als eigene Stufe NACH einem gescheiterten Standardlauf (kann nichts verschlechtern): an
  Blockgrenzen von Blöcken, die mit einem Namen beginnen, wird bei unplausibler Position der nächste plausible Anfang mit
  nicht leerem Namen im Umkreis ±16 Byte gesucht (NiSourceTexture -> benannte NiMaterialProperty u.ä.).
- 5 Dateien haben jetzt eine andere Dreieckszahl (mehr): `horse1.nif` 720 -> 32814, `Portal.nif` 5826 -> 7362 - vermutlich
  vollständigere Geometrie statt eines Teilmodells (die Strict-End-Prüfung bestätigt den Parse bis zur Fußzeile); optisch
  nicht geprüft.

**Noch offen (200 von 3429):** `NiNode` (~55), `NiBillboardNode` (25), `NiTriStrips` (~20), `NiPixelData` (17), `NiTriStripsData` (16),
`NiPSys*`-Effekte (reine Partikeldateien), `NiFlipController`, `NiSourceCubeMap`, `NiCollisionData`. Darunter echte Objekte
(`adl_town.nif`, `ElderinGround*.nif`, `stadium.nif`, `machine.nif`, `map-up.nif`). Bei ihnen liegt die Leseposition um
NICHT-4er-Beträge daneben (z.B. `EnvSet.nif`: NiVertexColorProperty liest 9 Byte zu wenig) - dort braucht es
blocktypspezifische Korrekturen, keine allgemeine Suche.

**Tests:** alle Suiten grün (Regressionsprüfung: `loadone`-Vergleich über alle 3429 NIFs, s. HANDOFF). NICHT geprüft:
Darstellung der neu ladbaren Modelle im echten Build (v.a. Mauern/Türme mit >100000 Vertices - Renderzeit), MSVC-Build.

## [0.44.30] — Charakter-Modelle (reschar): Avatar-Vorschau für Custom NPCs, NIF-Geometrie-Neuaufbau

Anlass: `reschar.zip` (Standard-Charaktermodelle) - Grundlage für "Spieler mit Rüstung" im Custom-NPC-Assistenten.

**Aufbau der Charaktermodelle (an Fighter-m/-f und Cleric-f geprüft):** Klassen 0 Fighter, 1 Archer (KEIN Ordner
in reschar), 2 Cleric, 3 Mage, 4 Joker, 5 Sentinel (Zuordnung aus `ChrCreateEquip`: Standardwaffen). Je Klasse und
Geschlecht ein Ordner `<Klasse>-<m|f>/` mit: Körper `<Klasse>-<m|f>.nif` (geskinnt, Bindepose = T-Pose, 3 Detail-
stufen je Teil Body/Legs/Shoes, ~50 Einheiten hoch), `Face00N.nif` (Gesichtshaut + Augen, KEIN Kopf im Körper),
`Hair...Front/Bottom/Top_L.nif` (Namen aus `HairInfo`), Rüstungs-Sets `setNNN.nif` (Nummer = `ItemViewInfo.MSetNo`/
`FSetNo`), Texturen `<ItemViewInfo.TextureFile>.dds`, Animationen `.kf`, Zusatzteile `<Item>_BR/_BELT/_UA/_CA/_CH/
_SH/_SHAC/_WC/_TAIL.nif`. Untexturierte Teile im Körper sind unsichtbare Knochen-Hüllen. Waffen: `ItemViewInfo.LinkFile`
-> `resitem/<LinkFile>.nif`.

**Neu: `AvatarPreview` (Core) + Vorschau im Assistenten** (Tab "Custom NPC/Mob", Modus "Spieler-Avatar mit
Rüstung", rechts neben den Ausrüstungs-Slots, mit Drehregler): `BuildAvatarModel` setzt Körper, Set-Geometrie je Slot
(Körper/Beine/Schuhe per Höhe des Schwerpunkts erkannt), Gesicht am Knoten `Bip01 Head`, Haare (HairInfo) und
Waffen zusammen; `RenderAvatarModel` rendert per CPU (Z-Buffer, Textur, Alpha-Test) - ohne GL testbar. Offline
gerendert und angesehen: Fighter männlich mit Brigandine- und Fury-Set. **Nicht dargestellt/geschätzt:** Zusatzteile
(_BR, _BELT, ...), Kopfbedeckung, Waffen-/Schildposition (Hand ±16 Einheiten, geschätzt), Archer (keine Modelle).
`NifModel::nodes` (benannte Knoten mit Weltposition/-rotation) neu. Gesicht/Haare werden OHNE Knochenrotation nur an
den Kopfknoten verschoben (Augen liegen im Modell bereits vorn bei -z und oberhalb der Haut).
Textur-V: `LoadDdsImage` liefert die Zeilen unten zuerst, NIF-UVs zählen von oben - Zeile = (1-v)*Höhe (sonst stand das
Gesicht kopfüber).

**NIF-Parser:**
- `NiBoneLODController` wird übersprungen (nif.xml; Charakter-NIFs).
- `ParseNiTriStripsHeader`: NUR der Fall `has_shader=1` liest jetzt zusätzlich "Shader Extra Data" (i32, -1) -
  behebt bei geskinnten Meshes ("FxSkinningBaseMap") den 4-Byte-Versatz; `has_shader=0` bleibt unverändert.
- **Geometrie-getriebener Neuaufbau der Parts:** das alte Verfahren legt EINEN Part je NiMaterialProperty an und
  füllt ihn mit dem nächsten Datenblock - bei mehreren Detailstufen/geteilten Properties entstanden Parts aus
  Vertices EINES Blocks mit Dreiecksindizes ALLER Blöcke (z.B. 24 Vertices, 218 Dreiecke). Jetzt: pro Block
  mitgeschrieben, bei Widerspruch aus den Geometrie-Knoten neu aufgebaut. Modelle mit Index hinter dem letzten
  Vertex: Feld-NIFs 77 -> 0, Karten-Ordner 42 -> 0, Item-NIFs 177 -> 0, Charakter-NIFs 97 -> 0; zusätzlich fehlende Teile
  (AgrippaWand 0 -> 348 Dreiecke, AlastoAxe 6 -> 243, stontree 1614 -> 5850). 0 Dateien verloren.
- Ladbar gegenüber v0.44.24: Charakter-NIFs 323 -> 713 von 725 (Stichprobe aus 3 Ordnern), Item-NIFs 1085 -> 1443 von 1503,
  Feld-NIFs 751 -> 822 von 854, Fixtures 70 -> 78 von 83.

**Tests:** neu `test_avatar_preview` (Fehlerfälle, Zusammensetzen, Renderpuffer); `test_nif_model` prüft die Konsistenz
der Parts. Alle Suiten grün; Headless-Tests (Assistent inkl. Avatar-Daten: 14976 ItemView-Einträge, FuryArmor mit
Textur/Set 1) und Software-Rendering des Assistenten-Layouts. NICHT geprüft: GL-Upload/Anzeige der Vorschau im echten
Fenster, MSVC-Build, restliche 7 reschar-Ordner.

## [0.44.29] — UI-Fehler (Popups, Zell-Editor, Quest-Editor), Sichtbarkeit + Kategorien, Block&Walk auf Zell-Ebene, mehr Objekte

Rückmeldungen nach dem Test von v0.44.28: Adelia sieht gut aus, Gras sichtbar; Item-Auswahl "öffnet ein
Fenster, das sich minimiert" (auch "anderes Modell wählen"); Custom NPC mit Spieler-Avatar lässt sich nicht
ausrüsten; SHN-Zelle: vorgegebener Name lässt sich trotz "Übernehmen" nicht ändern; Quest-Editor sieht
fehlerhaft aus; Sichtbarkeit von Objekten/Layern + Objekt-Kategorien gewünscht; Block&Walk bei Adelia falsch;
fehlende Objekte sollen nach und nach eingebaut werden.

**UI-Fehler (alle per Software-Rendering der ImGui-Oberfläche bzw. Headless-Klicktest nachgestellt):**
- *Such-Fenster "minimiert":* Popups sind AlwaysAutoResize; die Ergebnisliste hatte eine RELATIVE Höhe
  (0/-x) und schrumpfte das Fenster auf ein Minimum. Jetzt feste Größe (520x320). Betrifft Item-Auswahl im
  Shop-Editor, Item- und Modell-Auswahl im Custom-NPC/Mob-Assistenten.
- *Ausrüsten geht nicht:* `OpenPopup` stand INNERHALB von `PushID(i)`/Tabelle, `BeginPopup` außerhalb - andere
  ID, das Popup öffnete sich nie. Jetzt Merker + Öffnen auf derselben ID-Ebene.
- *Zelle "Übernehmen":* das Textfeld schrieb den getippten Text nur bei Enter in den Zustand, "Übernehmen"
  wendete den ALTEN Text an. Der Text wird jetzt nach jedem Aufruf übernommen (Test: tippen, dann Klick auf
  "Übernehmen" ohne Enter -> angewendet).
- *Quest-Editor:* neu als Formular (feste Labelspalte, kompakte Felder, aufgelöste Namen in einer eigenen
  Spalte), Monster-/Item-/Drop-Ziele als Tabellen; Text-ID -> Text per Hash-Map statt Linearsuche pro Quest und
  Frame; Liste mit ListClipper und gecachten Beschriftungen; Titel fällt auf die Beschreibung zurück
  (viele Quests haben nur diese), "-----"-Platzhaltertexte werden als "kein Text" gezeigt.
- Shop-Editor: "+ Zeile" überlagerte den Löschen-Haken (SameLine ohne folgendes Widget).

**Sichtbarkeit (Map-Editor, Bereich "Sichtbarkeit" im Werkzeug-Panel):** Terrain, Objekt-Modelle,
Objekt-Platzhalter, NPC-Modelle, Objekte im 2D-View; einzelne Terrain-Layer (Shader `uLayerVisible`);
**Objekt-Kategorien** (aus dem Modellnamen abgeleitet): Bäume & Büsche, Gras & Blumen, Felsen & Steine,
Gebäude, Zäune/Mauern/Brücken, Dekoration & Möbel, Wasser & Schiffe, Tiere & Kreaturen, Effekte & Licht,
Sonstiges - mit Anzahl, "nur diese", "Alle an/aus". Rou/Adl/Cypian: "Sonstiges" nur noch 33/2/0 Objekte.
Ausgeblendete Objekte werden weder gezeichnet noch im 2D-View angeklickt.

**Block&Walk (Adelia und alle Karten) - falsche Annahme korrigiert:** Ein Walk-Wort enthält 16 ZELLEN
(Bit 0 = linkeste Zelle, LSB-zuerst; Bit gesetzt = blockiert, Bit 0 = begehbar); eine Zelle ist IMMER
6.25 Welteinheiten groß, das Gitter quadratisch (Adl 951x476 Blöcke -> 7600x7600 Zellen, die Karte belegt nur
die ersten 3800 Zeilen). Belegt: Serverpunkte (NPCs, MapWayPoint) liegen bei Rou 120/120, Adl 18/18 und 89/89
auf Bit 0 (MSB-zuerst: 96 %/89 %/99 %); die frühere Streckung aufs Kartenformat lag bei Adl 0/18 richtig.
Neu: `WalkGrid::CellBlocked/SetCellBlocked/Cols/Rows/kCellSize`, zellgenauer Kreis-Stempel
(`ApplyWalkBitStamp`), Polygon-Stempel (`ApplyWalkConvexPolygon`), Vorschau-Textur auf Zell-Ebene mit
Teil-Update (`glTexSubImage2D`) und UV-Skalierung bei nicht-quadratischen Karten, Werkzeug "Grundflächen
sichtbarer Objekte sperren/freigeben" (nutzt die Sichtbarkeit/Kategorien). Gegenprobe: Gebäude-Grundflächen
liegen zu 85-97 % (Adl) auf gesperrten Zellen; die Objekt-Drehrichtung +theta (aktuell) passt in allen drei
Karten gleich gut oder besser als -theta (18:10, 2:0, 61:53). `test_walk_grid` prüft Zell-Ebene, Stempel, Polygon.

**Mehr Objekte:** (a) Teilmodell-Stufe: scheitert das Parsen in einem reinen Animations-/Partikel-/
Kollisionsblock NACH der Geometrie (NiPSys*, NiBool*, NiFlipController, NiCollisionData, ...), wird das bis
dahin Geladene verwendet. Adl: Objekte mit Mesh 1242 -> 1297 (0 nicht ladbar), Cypian 2290 -> 2295 (0),
Rou 1564/1580. (b) NiRangeLODData-Resync. Gesamt gegenüber v0.44.24: Feld-NIFs 751 -> 822 von 854, Item-NIFs
(resitem) 1085 -> 1429 von 1503, Fixtures 70 -> 78 von 83, 0 Dateien verloren. Offen: Rou_M_Banner/wool2
(NiTriStrips-Header - Parser bleibt tabu), animierte Item-NIFs (NiTransformController/Interpolator),
Modelle, die in den Beispiel-Zips fehlen (Adl 24, Cypian 89 Objekte).

**Geprüft:** alle Suiten grün; Headless-Tests (Zell-Übernehmen, Shop-Editor inkl. Item-Auswahl, Assistent,
SHN-Grid, Portale, Ordner); Software-Rendering von Quest-Editor, Assistent (mit Ausrüstungs-Popup) und
Shop-Editor. NICHT geprüft: Rendering/Klickverhalten im echten Build (GL), MSVC-Build, Verhalten des Servers.

## [0.44.28] — SHN-Bearbeiten, große Karten, NIF-Abdeckung + Höhen, Shop-Editor, Custom NPC/Mob

**SHN-Editor.** (a) "Nur eine Ziffer": der Zell-Editor gab ImGui einen Puffer von genau Textlänge+1
- bei "0" ging nur ein Zeichen, nie ein längerer Wert als vorher (auch die eigentliche Ursache von
"Zellen nicht bearbeitbar"). Jetzt 1024 Zeichen. (b) Neue Zeilen bekommen eine **automatische freie
ID**: erste ID im GRÖSSTEN freien Block über Quelldatei + Familien-Dateien (nicht "Maximum+1": ItemInfo
reicht bis 65504, MobInfo hat Sondernummern ab 50000) - ItemInfo 24083, MobInfo 15036, jede weitere
Zeile +1; dieselbe ID in der ganzen Datei-Familie (z.B. ItemViewInfo), InxName-Platzhalter
"Custom<Datei><ID>". (c) ListClipper + Filter-Cache: ItemInfo 278 ms -> 0.7 ms/Frame.

**Große Karten (Adl 951x476, Cypian).** Jeder Blend-Layer deckt laut .ini nur eine Region ab
(#StartPos_X/Y, #Width/#Height in Vertex-Einheiten; Adl: 476x476, Layer 10+ ab X=476) - der Renderer
legte jeden Layer über die ganze Karte (gestreckt). Außerdem waren nur 8 Layer möglich (Adl 10,
Cypian 12). Jetzt: `TextureLayer::regionStartX/Y/Width/Height` (aus der .ini), Blend-UV UND
Diffuse-Kachelung beziehen sich auf die Region, bis 24 Layer in Durchgängen zu 8 (additiv), Pinsel
rechnet in die Layer-Region um und normalisiert nur unter Layern derselben Region. Daten: Summe |Korr.|
Blend-Layer vs. Heightmap bei Adl 0.12 -> 2.71 (Layer 3: +0.67); bei Rou passt die .ini-Breite 257
minimal besser als 256 (1.5206 vs. 1.5137).

**NIF-Abdeckung und Objekt-Höhen.**
- NIF 10.2.0.0 Streifen-Ausrichtung war bei einem Teil der Dateien um 2 Byte verschoben (Vertices,
  0 Dreiecke -> Gras/Blumen/Pfähle unsichtbar). Selbstprüfend: Dreiecke == Σ Streifenlängen - 2*Streifen.
  Rou: Objekte mit Mesh 84 % -> 98.4 %.
- **Szenengraph:** Translation/Rotation/Skalierung aller NiNode/NiTriStrips/NiTriShape wurden gelesen,
  aber NIRGENDS angewendet (tree05: Y -1104..-10, Baum 2500 Einheiten im Boden). Jetzt Weltmatrix je
  Geometrie (Rotation zeilenweise, an Adl entschieden: 95 % vs. 60 % transponiert); NiLODNode zeichnet
  nur noch EIN Kind (vorher 3 Baum-Modelle übereinander). Anteil der Objekte, deren unterster Punkt
  ±30 Einheiten vom Gelände liegt: Rou 37 % -> 70 %, Adl 95 %, Cypian 76 % (Rest steht größtenteils auf
  anderen Objekten bzw. ist beabsichtigt eingegraben - Einschätzung, nicht geprüft).
- **Varianten-Suche** beim Laden (nur wenn der Standardlauf scheitert; Kriterium: fehlerfrei bis zur
  Fußzeile, ohne Textur-Dekodierung): NiMaterialProperty 14/15 Floats, 4 Byte zu viel/wenig nach
  NiPixelData, Leseposition ±4/±8 Byte an Blockgrenzen kurz vor der Fehlerstelle (Tiefensuche, max. 3);
  NiRangeLODData-Resync (Item-NIFs). Ladbar: Feld-NIFs 751 -> 807 von 854, Fixtures 70 -> 73/83,
  Feld-Ordner 335 -> 351/392, Item-NIFs (resitem) 1401/1503; 0 Dateien verloren. Verbleibend: NiTriStripsData,
  NiCollisionData, NiFlipController, NiBoneLODController, Partikel (NiPSys*).

**NPC-Shops (NPCItemList/<NPC>.txt)** - Editor neu: Tabs (Kategorien) anlegen/löschen, Zeilen (Regale) anlegen/
verschieben/löschen (nur nach Haken "Löschen freigeben"), Item-Auswahl mit Suche über ItemInfo (Name/
InxName/ID), unbekannte Item-Namen rot, Rechtsklick leert einen Slot, neue Shop-Datei für NPCs ohne
Datei. Alle Slots (auch Skill-/Titel-Händler) sind ItemInfo-InxNames. `SaveShineTextFile` kann jetzt
Tabellen anlegen (`ShineTable::isNew`) und entfernen (Kopf verschwindet mit) und neue Dateien aus einer
Vorlage erzeugen (`MakeShineFileWithHeaderOf`). Massentest 672 Dateien: Tabelle löschen 187/187, neue
Tabelle 672/672, unverändert speichern 677/677 byte-identisch.

**Custom NPC / Mob** (SHN-Editor, Tab "Custom NPC/Mob"): klont eine Vorlage in MobInfo (Client+Server),
MobInfoServer, MobViewInfo (Client + Server/View/), MobSpecies, QuestSpecies, MobWeapon mit einer in ALLEN
Tabellen freien ID (Zeilen ans Ende, Tabellen sind zeilenweise ausgerichtet). NPC-Look: Modell wählen
ODER "Spieler-Avatar mit Rüstung" = neue Zeile in NPCViewInfo.shn (Klasse 0-5, Geschlecht, Gesicht,
Frisur, Haarfarbe, 19 Ausrüstungs-Slots per Item-Auswahl), `MobViewInfo.NpcViewIndex` verknüpft. Optional:
Dialog kopieren (NpcDialogData), Platzierung + Rolle in World/NPC.txt. "Alle geänderten SHN speichern".
Zusätzlich im NPC-Tab: Rolle/Rollenargument bearbeitbar. NICHT geprüft: ob das Spiel die neuen Einträge
akzeptiert; keine 3D-Vorschau des Avatars (Körpermodelle fehlen; Item-NIFs liegen in resitem/).

**Tests:** neu/erweitert: `test_shinetext` (Tabellen anlegen/löschen, neue Datei aus Vorlage),
`test_nif_model` (Szenengraph). Alle Suiten grün; Headless-Tests (SHN-Grid, Neue-Zeile-ID + Tippen,
Shop-Editor inkl. Item-Auswahl/Speichern/Neuanlage, Assistent inkl. Speichern+Neuladen). NICHT geprüft:
Rendering/Klickverhalten im echten Build (GL), MSVC-Build der neuen Teile.

## [0.44.27] — Spiegel-Ursache gefunden (Textur oben/unten, Objekte links/rechts), Objekt-Texturen, Pfade, SHN, Mob-Spawns

Nutzer-Meldungen (Screenshots/Beschreibung): Textur oben/unten vertauscht, Objekte links/rechts
vertauscht ("was links auf der Karte steht, steht rechts"), Objekt-Texturen nicht bunt, SHN-Zellen
nicht bearbeitbar, Quest-Editor lädt nicht, Pfade sollen sich aus Client-/Server-Ordner ergeben,
Portale müssen gerendert werden, Grundfläche im Block/Walk-Modus ungenau, Mob-Spawns konfigurierbar.

**1. Spiegelung: gemeinsame Ursache + Korrektur.** Legacy ist Z-up (x,y,z), intern wird Y-up
verwendet über reines Vertauschen (x,y,z)->(x,z,y) - das ist eine SPIEGELUNG (det -1). Empirisch
(Karte Rou, weitere Karten stichprobenartig) belegt:
- Objekt-Höhen gegen Heightmap unter allen 8 Achsenabbildungen: Identität (col=X, row=Z) Median-
  Fehler 4.9 (Adl: 0.0), jede andere >= 129. Walk-Grid und Server-Koordinaten (NPC.txt,
  MapWayPoint.shn: 89 % der Punkte auf freien Walk-Wörtern statt 15.6 % Zufall) liegen im selben
  Rahmen. -> Heightmap, Objekte, Walk-Grid, Server-Koordinaten sind untereinander konsistent.
- Blend-Layer und Vertex-Color-Bitmap passen zu diesem Rahmen NUR mit vertikalem Flip (Layer
  "rock" Korrelation mit Höhe +0.79 vs. -0.06; Cypian: Wasser/Sand-Layer ebenso). Ursache war
  der Bottom-up-Flip in `BmpBlendMap`. **BMP-Zeilen werden jetzt in Dateireihenfolge gelesen/
  geschrieben (Zeile y = Gitterzeile z).** Byte-Exaktheit der Exporte bleibt (beide Richtungen).
- Objekt-Rotation: Import/Export negiert den Vektoranteil des Quaternions ((x,y,z,w) ->
  (-x,-z,-y,w) bzw. zurück) - vorher drehte jedes Objekt in die falsche Richtung. 2D-Grundfläche
  entsprechend angepasst. NPC-Platzhalter-Objekte (aus NPC.txt-Richtung) bewusst UNVERÄNDERT.
- **Ansicht:** 3D-Kamera spiegelt Z vor der Anzeige (`OrbitCamera::ViewMatrix`; Kamera-Ziel/Yaw/
  Pan im Anzeigeraum, `SetTarget`/`TargetZ` weiter in Welt-Koordinaten). Kamera nach Norden:
  Osten rechts, Norden oben (`test_camera_handedness`). 2D-View: Norden (größeres Z) OBEN - Bild
  ohne UV-Vertauschung, alle Marker (Objekte, NPCs, Mob-Zonen, Portale, Grundflächen) und die
  Maus-Umrechnung mit v = 1 - z/spanZ. **ANNAHME (nicht am Spiel geprüft):** im Spiel liegt +X
  rechts und +Y (Legacy) oben; gestützt darauf, dass die Karten-BMPs so aufrecht sind.

**2. Objekt-Texturen (eingebettete NiPixelData) waren Rauschen.** (a) Ab NIF 10.4.0.2 folgt auf
"Num Pixels" das Feld "Num Faces" (u32), erst danach die Pixeldaten (nif.xml) - die Daten lagen um
4 Byte verschoben (Endpunkte benachbarter DXT-Blöcke nur bei Versatz 4 glatt). (b) "Bytes Per
Pixel" 3/4 = unkomprimiertes RGB/RGBA (bisher fälschlich als DXT dekodiert). Leseposition des
Parsers unverändert (Header-Parser bleibt tabu). Ergebnis: verrauscht 400 -> 3 von 933 Texturen
(Fixtures 114 -> 0 von 154); ladbare NIFs unverändert 751/854 bzw. 70/83. Stichproben als PNG
kontrolliert (Seil, Zaun, Hai, Himmel, Statue).

**3. Pfade:** `SyncProjectRoots` leitet aus Client-/Server-Ordner des Projekts `ressystem` und
`Server/9Data/Shine` ab (12 Angabe-Varianten getestet); NPC/Mob/Portale/Quest/SHN-Editor nutzen
sie. SHN-Editor liest beim ersten Öffnen beide Ordner ein. Quest-Editor: Fallback-Suche, klare
Fehlermeldung (Ursache war der manuell zu wählende Ordner). **SHN-Grid:** ImGuiListClipper +
Filter-Cache (ItemInfo.shn 278 ms -> 0.7 ms pro Frame), Knopf "Zelle bearbeiten". Hinweis: die
Ursache "Zellen nicht bearbeitbar" ist nur vermutet (Performance); im Headless-Test mit festen
Frames ging der Doppelklick auch im alten Code.

**4. Gelb im SHN-Editor:** ⚠ an einer Datei = eine andere geladene SHN hat denselben Namensstamm
oder dieselbe Zeilenanzahl (>20) - Hinweis auf gemeinsam zu pflegende Dateien (docs/
SHN_DEPENDENCIES.md), kein Fehler. Gelbes ◆ im Multi-Editor = Kandidat der gewählten Aufgabe.

**5. Portale im 3D-View** (Tab "Portale"): große Marker (TownPortal 4x, Schriftrolle 3x, gewähltes
weiß) über dem Gelände.

**6. Grundfläche:** konvexe Hülle der untersten Höhenschicht (12 % der Modellhöhe, 20..120 Einh.)
statt Bounding-Box (`ComputeFootprintHull`): Baum 5 % der Box (Stamm), Hütten 62-64 %. Konvex -
L-förmige Grundrisse werden überdeckt.

**7. Mob-Spawns konfigurierbar** (Tab "Mobs"): alle Spalten der Zone (Center/Größe/RangeDegree/
IsFamily) und jedes Monsters (Name, Anzahl, Kill-Anzahl, RegStandard/Min/Max, Delta/Sec) editierbar;
Monster hinzufügen, Zone kopieren; Löschen (Monster/Zone samt Monstern) nur nach Haken "Löschen
freigeben". NICHT geprüft: ob der Server per `#recordin` eingefügte MobRegen-Zeilen liest.

**8. `SaveShineTextFile` (Datenfehler behoben):** entfernte Records blieben in der Datei stehen; ein
Einfügen verschob die Zeilenindizes späterer Tabellen, sodass dort spätere Änderungen in FALSCHE
Zeilen geschrieben wurden (MobRegen mit zwei Tabellen). Jetzt: Ersetzen in-place, Löschen/Einfügen
in einem Ausgabe-Durchlauf (`ShineTextFile::loadedRecordLines`). Massentest über 672 Dateien:
Löschen 672/672 (vorher 0), Hinzufügen 672/672 (495), Einfügen+Bearbeiten in 2. Tabelle 184/184
(2). Unverändert speichern weiter 677/677 byte-identisch.

**Geprüft:** alle Suiten grün (neu: `test_camera_handedness`, BMP-Rohreihenfolge, Hülle, Löschen/
Mehrtabellen in `test_shinetext`); `-fsyntax-only -Wall -Wextra` über `src/app/*.cpp`; Headless-
ImGui-Tests (SHN-Grid, Feld-Editor, Ordner, Portale). **NICHT geprüft:** Rendering/Klickverhalten
im echten Build (GL), MSVC-Build der neuen Teile, Verhalten des Servers bei geänderten Dateien.

## [0.44.26] — Build-Fix: `QuestData.cpp` fehlte in der CMake-Quellenliste

Der erste echte MSVC-Build von v0.44.25 (Windows, `cmake --build build --config Release`) lief
durch die Compile-Phase ohne Fehler - insbesondere kompilierten `main.cpp` und damit auch die drei
in [0.44.25] korrigierten `#ifdef _WIN32`-Blöcke - scheiterte aber beim Linken von `map_editor.exe`
und `test_questdata.exe` mit LNK2019/LNK1120 auf `LoadQuestData`/`SaveQuestData`. Ursache:
`src/core/legacy/QuestData.cpp` (seit [0.44.19]) stand nicht in `add_library(mapeditor_core ...)`.
Ergänzt. Geprüft: alle `src/**/*.cpp` sind jetzt in `CMakeLists.txt` aufgeführt; `test_questdata`
ohne die Datei reproduziert die undefinierten Symbole, mit ihr linkt und besteht er
(`QuestData.shn`, 0 Fehler). Kein Code geändert. NICHT geprüft: der komplette MSVC-Build danach
(unter Linux ist nur GCC verfügbar), und wie die App zur Laufzeit aussieht.

## [0.44.25] — Portale-Tab (TownPortal/Schriftrollen), SHN-Spaltennamen-Bug, exakter ShineText-Writer

**Roadmap-Punkt 1 (TownPortal) geklärt und umgesetzt.** Frage war: sind `TownPortal.shn` X/Y ein
Ort AUF einer Karte oder nur ein Reiseziel? Antwort, gegen die echten NA2016-Dateien geprüft:
**ein Ort auf der Karte**, im selben Weltkoordinaten-System wie `World/NPC.txt` (Coord-X/Y) und
`MapInfo.shn` (RegenX/Y).
- Alle 8 TownPortal-Einträge liegen höchstens ~205 Einheiten vom `Gate_Town`-NPC derselben Karte
  entfernt (RouN exakt identisch: 5874/6530; RouVal01 50, Eld 72, EldGbl02 30, Urg 96,
  Urg_Alruin 205, Adl 94, Bera 98).
- `RecallCoord.txt` (`RecallPoint`: ItemIndex/ItemIdent/MapName/LinkX/LinkY) nutzt dieselben
  Weltkoordinaten; bei den Haupt-Städten (RouN, Eld, Urg) ist LinkX/Y identisch zu RegenX/Y aus
  `MapInfo.shn`, bei anderen Karten (z.B. EldGbl02, Urg_Alruin) nicht.
- Client- und Server-Kopie von `TownPortal.shn` sind byte-identisch.

**Neuer Tab "Portale"** (`EditMode::Portals`) im Map-Editor:
- 2D-View: TownPortal = Raute, Schriftrolle = Dreieck, `Gate_Town`-NPC = Quadrat (mit dünner
  Linie zum TownPortal-Ziel), Wiederbelebungspunkt = Ring. Auswahl per Klick oder Liste.
- Bearbeiten: X/Y (Eingabefelder oder "Position per Klick im 2D-View setzen"), Mindestlevel und
  Menü-Gruppe (TownPortal), Abstand zum Gate_Town-NPC wird angezeigt.
- "TownPortal-Ziel hier hinzufügen": neue Zeile in `TownPortal.shn` (Kopie der letzten Zeile,
  nächster freier Index, Startposition Gate_Town-NPC > Regen-Punkt > Kartenmitte).
  **Ungeprüft**, ob der Client für neue Ziele weitere Daten braucht.
- Speichern: `TownPortal.shn` in Client (`ressystem/`) UND Server (`Server/9Data/Shine/`, nur
  wenn die Datei dort schon existiert); `RecallCoord.txt` in `World/`.
- Nicht umgesetzt: 3D-Marker, Löschen von TownPortal-Einträgen, Hinzufügen von Schriftrollen
  (hängt an `ItemInfo`).

**Bug in `ShnFile` behoben (Datenverlust beim Speichern):** Spaltennamen mit < 2 Zeichen wurden
beim Laden zu "Undefined N" umbenannt und beim Speichern so geschrieben. Die echten Spalten von
`TownPortal.shn` und `MapWayPoint.shn` heißen `X`/`Y`. Jetzt bekommen nur echt leere Namen einen
Platzhalter (`ShnColumn::synthesizedName`, wird beim Speichern wieder leer geschrieben).
**Round-Trip aller SHN-Dateien (Client+Server, 350): vorher 299/348 byte-identisch, jetzt 348/348**
(die 2 nicht ladbaren sind `QuestData.shn`, eigenes Format). Folge für Roadmap Punkt 2:
`MapWayPoint.shn` hat die Spalten `MapID`, `X`, `Y`, `MWP_Gate` - die "unbenannten Felder"
waren dieser Artefakt. Die Bedeutung von `MWP_Gate` ist weiterhin NICHT geklärt.

**`SaveShineTextFile` exakt gemacht:** bisher wurde JEDE Zeile mit CRLF geschrieben und bei jedem
Record die Auffüll-Tabs verworfen (unveränderte Datei: 0 von 677 Textdateien aus
`Server/9Data/Shine` byte-identisch). Jetzt: Zeilenende je Zeile gemerkt
(`ShineTextFile::rawLineEndings`, neue Zeilen nutzen `defaultLineEnding`), Auffüll-Tabs und das
Leer-Tab nach `#record` bleiben erhalten. **Unverändert speichern: 677/677 byte-identisch.**
Eine Bearbeitung ändert genau die eine Zeile (an `RecallCoord.txt` geprüft). `test_shinetext`
prüft jetzt zusätzlich die Byte-Identität. Betrifft auch NPC.txt/MobRegen/MobRoam/NPCItemList.

**Weitere Korrekturen in `main.cpp`:**
- `p->string()` auf `std::optional<std::string>` in drei Ordner-Dialogen (ressystem, NPC, Mob)
  war ein Windows-Compile-Fehler (versteckt unter `#ifdef _WIN32`) - jetzt `*p`.
- Klick im 2D-View bearbeitete die Heightmap in Tabs ohne eigene Klick-Logik (NPC AI, Mob AI,
  NPCs/Mobs ohne geladene Daten). Pinsel gilt jetzt nur noch im Heightmap-Tab, Walk-Stempel nur
  im Block/Walk-Tab.
- `RecallCoord.txt` wird auch über `shineTextRoot` gefunden (nicht nur `shnServerRoot`).

**Geprüft:** alle 9 Test-Suiten grün (mit echten Referenzdaten); `-fsyntax-only -Wall -Wextra`
über alle `src/app/*.cpp` mit echten ImGui-Docking-/GLFW-3.4-/glad-Headern sauber; Headless-Test
der Portal-Logik gegen echte NA2016-Daten (Laden, alle 8 TownPortal-Karten, Bearbeiten,
Hinzufügen, Speichern nach Client+Server, Neuladen). **NICHT geprüft:** Rendering/Klickverhalten
im echten Build (GL), die drei Windows-Dialoge (unter `#ifdef _WIN32`, unter Linux nicht
kompilierbar), Verhalten des Servers bei geänderten Dateien.

## [0.44.24] — Echte Objekt-Grundfläche im 2D-View (aus NIF-Vertex-Daten)

Löst die in [0.44.23] genannte Einschränkung: Objekte zeigten dort nur ihre POSITION als
Punkt, keine echte Ausdehnung. Jetzt: `GetOrComputeFootprint` berechnet (gecacht pro
`modelPath`) eine Bounding-Box aus den ECHTEN NIF-Vertex-Positionen (`core::NifMeshPart::
positions`, bereits Y-up, X/Z = horizontale Ebene) - `DrawObjectFootprint2D` zeichnet daraus
ein Y-rotiertes, skaliertes Rechteck (Rotation aus dem Objekt-Quaternion, nur Y-Achse - X/Z-
Kippung ist bei platzierten Objekten praktisch immer 0) an der richtigen Weltposition. Löst
Modelle über denselben Kartenordner auf wie der 3D-Renderer (`state.legacySaveDir` +
`ResolveLegacyAssetPath`). Gegen zwei echte Dateien geprüft: `tree05.nif` liefert eine
plausible ~985×991 Einheiten große Baumkronen-Fläche; ein NICHT ladbares NIF (bereits bekannter
Fall aus den 811 offenen Parser-Lücken) liefert sauber `valid=false` statt abzustürzen - der
Aufrufer fällt dann auf den bisherigen Punkt-Marker zurück, kein Fehlerfall.

Sichtbar sowohl im Objekt-Platzierungs-Modus (Weiß/Blau, passend zur Auswahl) als auch im
Block/Walk-Modus (dezent, als Referenz beim Setzen der Lauf-Sperren, siehe [0.44.23]).

Nur syntaktisch geprüft (siehe [0.44.9]) plus eigenständig gegen echte NIF-Dateien verifiziert
(reines Bounding-Box-Rechnen, kein GL nötig). Keine Core-Änderung - alle 9 Test-Suiten
unberührt.

## [0.44.23] — Objekte im Block/Walk-2D-View sichtbar + Patrouillenrouten-Editor (NPC-KI)

**Objekte im Block/Walk-Modus sichtbar** (Nutzerwunsch: "damit man nicht durch Häuser läuft"):
platzierte Objekte werden jetzt auch im Block/Walk-2D-View eingezeichnet - bisher NUR im
Objekt-Platzierungs-Modus sichtbar. Bewusst dezent (halbtransparenter Punkt + dünner Ring
statt der vollen Weiß/Blau-Marker) und NICHT anklickbar/verschiebbar in diesem Modus - reine
visuelle Referenz beim Malen der Lauf-Sperren, versehentliches Verschieben eines Gebäudes
beim Grid-Bearbeiten wäre schlimmer als gar keine Anzeige. **Bekannte Grenze:** zeigt nur die
Objekt-POSITION als Punkt, keine echte Gebäude-Grundfläche (dafür bräuchte es die NIF-
Bounding-Box, die aktuell nicht an die 2D-UI durchgereicht wird) - hilft aber schon deutlich
beim Erkennen, wo überhaupt etwas steht.

**Patrouillenrouten-Editor** (Punkt 4 der Nutzer-Roadmap, NPC-KI): neuer Button
"Patrouillenroute bearbeiten" im NPC-Tab sowie ein "Route"-Knopf pro Monster in der Mob-Spawn-
Zonen-Liste - lädt/bearbeitet `MobRoam/<Name>.txt` (Wegpunkt-Liste: ID/X/Y/EventIndex, "return"
markiert typischerweise die Rückkehr zum Start). Byte-exakt gegen echte Daten geprüft
(`BerValeDw04.txt`, 9 Wegpunkte). Reine Listen-Bearbeitung (Hinzufügen/Entfernen/Werte ändern)
- noch KEINE 2D-Visualisierung der Route als Linie auf der Karte, das wäre der naheliegende
nächste Schritt.

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 9 Test-Suiten unberührt.

## [0.44.22] — Echte KI-Skripte gefunden: Lua (404 Dateien) + PineScript (54 Dateien)

**Großer Fund** (Nutzerhinweis): neben den bisher gefundenen Verhaltens-Tabellen
(`MobRoam`/`MobAttackSequence`/`MobSetting`) gibt es zwei VOLLSTÄNDIGE Skriptsprachen für
Mob-/NPC-KI: `Server/9Data/Shine/LuaScript/AIScript/<Name>.lua` (404 Dateien, ECHTER Lua-Code
mit `require`/`function`/Engine-Aufrufen wie `cStaticDamage_smo`, `cSetServantFlag`,
`cAIScriptSet` - z.B. für `Chimera`, `BH_Humar`, `Toryming`) und
`Server/9Data/Shine/MobBehaviorDescript/**/*.ps` (54 Dateien, "PineScript" - eine eigene
Zustandsmaschinen-Sprache mit `open`/`close`-Blöcken, `if`/`then`/`else`, Befehlen wie
`whoistarget`/`permillage`/`chat`/`whokillme`, hauptsächlich für Kingdom-Quest-Bosse und
Instanz-Encounter). `World/PineScript.txt` (ShineText-Format, lädt bereits mit dem
bestehenden Parser) ist das Register aller beim Serverstart geladenen PineScript-Dateien,
teils mit Kommentar-Hinweis auf eine zusätzliche Lua-Datei.

**Bewusste Entscheidung:** kein Lua-Interpreter, kein PineScript-Parser - beides sind
vollständige (Skript-)Sprachen, deren Syntaxprüfung/Interpretation weit außerhalb dessen liegt,
was ein Karten-/Daten-Editor leisten sollte. Stattdessen: reiner Text-Editor
(`DrawAiScriptEditorPopup`), byte-exakt Laden/Speichern ohne jede Interpretation.

**Verknüpfung:** Namensauflösung gegen echte Daten verifiziert - "Chimera" existiert als
echter Mob-Name in `MobViewInfo.shn` UND als `Chimera.lua`, Dateiname entspricht 1:1 dem
Mob-InxName. Neuer Button "KI-Skript (Lua) bearbeiten" im NPC-Tab (neben "Dialog bearbeiten"),
und im Mob-Tab ist jedes Monster in einer Spawn-Zone jetzt anklickbar (öffnet direkt dessen
Lua-Skript, falls vorhanden - viele gewöhnliche Feldmonster haben keins, nur Bosse/Spezial-
Mobs, das ist normal und wird als Statusmeldung angezeigt, kein Fehler).

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 9 Test-Suiten unberührt.

## [0.44.21] — Quest-Editor: Item-/Mob-Namensauflösung dient zugleich als Validierung +
    Recherche zu den 7 neuen Nutzeranfragen (NPC-/Mob-KI, Waypoints, Custom NPCs, Belohnungen)

**Item-/Mob-Namensauflösung im Quest-Editor** (Start-NPC, Monster-Ziele, Item-Ziele, Drops,
benötigtes Item, Vorgänger-Quest): jede ID zeigt jetzt direkt den aufgelösten Namen aus
`ItemInfo.shn`/`MobViewInfo.shn` GRÜN, oder ROT falls die ID nicht existiert - erfüllt damit
zugleich einen Teil der Validierungs-Anfrage (ungültige Referenzen fallen sofort auf, ohne
extra Prüf-Knopf). Belohnungs-Bytes (144 Byte, Struktur seit [0.44.19] unklar) bewusst NICHT
geraten - ein Versuch, sie über `CREATE_ITEM`-Aufrufe in Quest-Skripten gegenzuprüfen, ergab
kein zuverlässiges Muster (siehe unten).

**Recherche zu den 7 neuen Punkten** (Umsetzung teils vertagt, siehe HANDOFF.md für Details):
- **NPC-/Mob-KI** (Punkte 4+7): `MobRoam/<Name>.txt` (Patrouillenrouten, Koordinatenliste +
  `return`-Marker) und `MobAttackSequence/<Boss>.txt` (nummerierte Angriffs-Skill-Sequenzen,
  nur für Bosse) gefunden und mit dem ShineText-Parser geladen - beide passen ins bestehende
  Format, noch keine UI dafür gebaut. `MobSetting/Action/<Name>.txt` (Trigger-Bedingung→Aktion,
  u.a. auch für Gate-Objekte wie `WarH_EntranceGate`) als drittes, verwandtes System gefunden.
- **Custom NPCs/Mobs** (Punkt 6): technisch machbar mit bereits vorhandener Infrastruktur
  (`World/NPC.txt` + `MobCoordinate.shn`/`MobRegen` + neuer Eintrag in `MobViewInfo.shn` fürs
  Modell) - noch nicht als koordinierter "Neu anlegen"-Workflow umgesetzt.
- **TownPortal im Map-Editor platzieren** (Punkt 3): noch offen - bräuchte eigene 2D/3D-Marker
  analog zu NPCs/Mobs, `TownPortal.shn` hat aber keine Karten-Bindung im selben Sinn (X/Y sind
  wohl das ZIEL, nicht ein Ort AUF einer Karte, der eine Platzierung bräuchte - genauer zu
  klären, siehe HANDOFF.md).
- **MapWayPoint für Auto-Move** (Punkt 5): `MapWayPoint.shn` (14886 Zeilen: `MapID`, zwei
  unbenannte Zahlenfelder, `MWP_Gate`) gefunden, `MapID`→Kartenname läuft über `MapInfo.shn` -
  Bedeutung der unbenannten Felder und `MWP_Gate`-Werte noch nicht verifiziert.

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 9 Test-Suiten unberührt.

## [0.44.20] — Portal-Taxonomie geklärt (Nutzer) + Editor für TownPortal/RecallCoord

**Vom Nutzer klargestellt, gegen echte Daten verifiziert - fünf Portal-/Teleport-Kategorien:**
1. **TownPortal** (`TownPortal.shn`, 8 Zeilen) - Skill mit AUSWÄHLBAREM Ziel (Menü).
2. **Normale Portale** (`World/NPC.txt`, `Role=Gate`, 199 Einträge) - feste Map-A→Map-B-Links.
3. **NPC-förmige Teleporter** (z.B. für PVP-Karten/Level-Gebiete) - dieselbe Gate-Mechanik wie
   2., nur mit einem Wächter-/Charaktermodell statt eines Torbogens als Optik.
4. **Schriftrollen mit festem Ziel** (`World/RecallCoord.txt`, Tabelle `RecallPoint`) bzw.
   Items mit wählbarem Ziel (vermutlich dieselbe Menü-Mechanik wie TownPortal, itemseitig
   ausgelöst - nicht weiter verifiziert).
5. **Instanz-Tore** - beim genaueren Hinsehen in `NPC.txt` als EIGENE Rollen `IDGate` (9) und
   `ModeIDGate` (4) gefunden ("ID" = Instance Dungeon) - Ziele wie `WarBL01`/`WarL01`/`WarH01`/
   `IDGate01`, klassische Dungoen-Eingangs-Namenskonvention.

**Zusätzlich beim Durchsuchen aller `NPC.txt`-Rollen gefunden:** `RandomGate` (5, führt zu
`GBHouse01`-`05` - Gilden-Häuser) und `NPCMenu`/`ClientMenu` (Gilden-Verwaltung, Münz-Automat -
KEINE Teleporter, reine Menü-NPCs). Rollen 2/3/5 sind bereits automatisch über die bestehende
NPC-3D-Pipeline abgedeckt (jede `World/NPC.txt`-Zeile wird unabhängig von ihrer Rolle geladen) -
kein zusätzlicher Code nötig.

**Neuer Editor** für die zwei bisher fehlenden, tatsächlich eigenständigen Tabellen (1. und 4.):
sechster Tab "Portale" im SHN-Editor, zeigt `TownPortal.shn` (Client) und `RecallCoord.txt`
(Server) direkt bearbeitbar. Beide sind klein genug (8 bzw. ~15 Zeilen) für eine einfache
Tabellen-Inline-Bearbeitung ohne weitere Aufbereitung.

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 9 Test-Suiten unberührt.

## [0.44.19] — Quest-Editor (QuestData.shn, vom Nutzer bereitgestellter Referenzparser) +
    Gate/Portal-Bestandsaufnahme

**QuestData.shn** ist ein eigenständiges, drittes Datenformat (weder das binäre .shn-
Containerformat noch ShineText): uint16 Header(0x0006) + uint16 QuestCount, variabel lange
Records mit eigenem uint32-Längenpräfix. Der Nutzer hat einen vollständigen, selbst
geschriebenen Python-Referenzparser bereitgestellt - **funktioniert zu 100% gegen die echten
NA2016-Daten** (Client UND Server, je 2304 Quests, kein einziger Ausrichtungsfehler). Nach
C++ portiert (`QuestData.hpp/cpp`, gleiche Architektur wie `ShnFile`/`ShineText`) und selbst
verifiziert: unverändertes Speichern ist **byte-identisch** zum Original (übertrifft sogar
ShineText, das kleinere Formatierungs-Normalisierungen hatte), Bearbeiten+Speichern+Neuladen
funktioniert korrekt, andere Quests bleiben unverändert. Die 144-Byte-Belohnungs-Slots (Struktur
laut Referenzparser noch nicht kartiert) werden unverändert durchgereicht, nicht geraten. Neue,
neunte Test-Suite `test_questdata.cpp`.

**Zusätzlicher Fund:** `QuestDialog.shn` (Client, 25222 Zeilen: ID→Text) löst die Zahlen-IDs in
`title`/`description` und den `SAY`-Befehlen der Quest-Skripte zu lesbarem Text auf, inkl.
`[BUTTON]=[Label][Aktion]`-Markup (ähnlich dem NPC-Dialog-Format aus [0.44.16]) - byte-exakt an
Quest 1 verifiziert (ID 200 = Titel "Baby Steps", ID 202-205 = die zugehörigen NPC-Dialogzeilen).

**Quest-Editor-UI:** der bisherige Platzhalter-Tab "Quest Editor" im SHN-Editor zeigt jetzt eine
durchsuchbare, nach aufgelöstem Titel filterbare Quest-Liste + Detail-Editor (Level-Range,
Start-NPC, 5 Monster-Ziele, 10 Item-Ziele, variable Drop-Liste, die drei Skripte Start/Action/
Finish als Textfelder, eigenständige Text-ID-Nachschlagefunktion gegen QuestDialog.shn).
Speichert minimal-invasiv über die neue `SaveQuestData`.

**Gate/Portal-Bestandsaufnahme** (Nutzerfrage zu "Town Gates" und "normalen Portalen"):
bestätigt - `TownPortal.shn` (8 Zeilen) ist die Skill-/Schriftrollen-Schnellreise-Liste,
"normale Portale" sind die bereits geladenen `Role=Gate`-Einträge in `World/NPC.txt`. Diese
laufen automatisch durch dieselbe NPC-3D-Rendering-Pipeline wie normale NPCs (siehe [0.44.17]/
[0.44.18]) - kein zusätzlicher Code nötig. Systematisch gegen die lokale (nur teilweise
vorhandene) Nif-Bibliothek geprüft: von 24 eindeutigen `Gate`-Einträgen im ganzen Spiel sind
8 Modelle bereits vorhanden (z.B. `MapLinkGate.nif`), **16 fehlen** - siehe Chat-Antwort für
die vollständige Liste (u.a. `LevelGuard00`-`LevelGuardH7` - vermutlich Level-Sperren, keine
Teleport-Portale im engeren Sinne, aber ebenfalls `Role=Gate`). Auch `RecallCoord.shn`
(Schriftrollen-Zielkoordinaten) und `Field.txt` (Karten-Metadaten inkl. `LinkIN`/`LinkOut`-
Flags) als verwandte, aber separate Systeme identifiziert und bewusst nicht weiter vertieft.

Nur syntaktisch geprüft (siehe [0.44.9]). Core-Änderung (neues Modul QuestData) - alle 9
Test-Suiten grün.

## [0.44.18] — Korrektur: Charaktermodelle liegen in "reschar", nicht in resmap/nif(s)

**Wichtiger Fund vom Nutzer** (zwei Screenshots des echten Client-Ordners): NPC-/Charakter-
Modelle liegen NICHT in der normalen `resmap/nif`- oder `nifs`-Bibliothek, sondern in einem
eigenständigen `Client/reschar/`-Ordner - pro Charakter ein eigener Unterordner mit exakt
gleichnamiger `.nif`-Datei (z.B. `reschar/AdlSmithAlexia/AdlSmithAlexia.nif`), daneben auch
Animationsdateien (`.kfm`, `.kf`, z.B. `AdlSmithAlexia_Bip01_Idle.kf`). Das erklärt, warum
[0.44.17]s Modell-Suche in `state.availableNifFiles` (deckt nur `resmap/nif(s)` ab) für
Charaktere IMMER ins Leere gelaufen wäre - `state.availableNifFiles` enthält diesen Ordner gar
nicht. Ohne den Hinweis wäre das ein stiller, schwer zu findender Fehler geblieben (keine
Fehlermeldung, NPCs hätten einfach nie ein 3D-Modell bekommen).

**Fix:** `EnsureRescharRoot` sucht den `reschar`-Ordner als Geschwister von `resmap`/
`ressystem` unter dem Projekt-Client-Ordner (gleiches Suchmuster wie `ressystem`/
`npcDialogRessystemRoot`). `EnsureNpcModelsLoaded` probiert zuerst `reschar/<Name>/<Name>.nif`,
fällt erst danach auf die normale `resmap/nif(s)`-Bibliothek zurück (für Nicht-Charakter-
"NPCs" wie `MapLinkGate`/`Anvil`, die ganz normale Props sind). Da beide Fälle in EINEM Aufruf
von `LoadModelsForSet` (nimmt nur EIN gemeinsames `mapDir`) landen müssen, werden beide Pfade
relativ zum Client-Ordner ausgedrückt und über denselben "fiktiver zwei-Ebenen-tiefer mapDir"-
Trick aufgelöst, der schon bei der Asset-Vorschau (`GetOrLoadAssetThumbnail`) verwendet wird -
mit einer synthetischen Ordnerstruktur verifiziert (`ResolveLegacyAssetPath` löst
`reschar/AdlSmithAlexia/AdlSmithAlexia.nif` korrekt zum echten Pfad auf).

**Zusätzlich geprüft:** der NIF-Parser überspringt `NiSkinInstance`/`NiSkinData`/
`NiSkinPartition` (Skelett-/Skinning-Daten) bereits byte-korrekt (Position bleibt richtig),
wendet sie aber NICHT auf die Vertex-Positionen an (keine Animations-Deformation). Charakter-
modelle rendern damit in ihrer im NIF gespeicherten Ruhepose, ohne die `.kf`-Animationen -
für eine Platzierungs-Vorschau im Map-Editor eine akzeptable Einschränkung, volle Animation
wäre ein eigenes, deutlich größeres Vorhaben.

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 8 Test-Suiten unberührt.

## [0.44.17] — NPCs vollständig in 3D gerendert + Mob-Spawn-Zonen mit Icon+Radius im 2D-View

**NPC-3D-Rendering:** neuer, eigenständiger zweiter `NifMeshRenderer` (`npcMeshRenderer`) -
getrennt vom bestehenden Objekt-Renderer, da `LoadModelsForSet` pro Aufruf sein internes
Modell-Set komplett neu aufbaut (ein gemeinsamer Renderer für Objekte UND NPCs würde sich
gegenseitig überschreiben). Auflösungskette recherchiert und byte-exakt gegen echte NA2016-
Daten verifiziert: `World/NPC.txt`s NPC-Name → `MobViewInfo.shn` (Spalte `InxName`) →
`FileName`-Spalte = Name der `.nif`-Datei. **Wichtiger Fund dabei:** `MobViewInfo.shn` hat
zusätzlich eine `NpcViewIndex`-Spalte - bei `!=0` ist die Erscheinung aus einzeln ausgerüsteten
Gegenständen zusammengesetzt (`NPCViewInfo.shn`, komplettes Ausrüstungs-System wie bei
Spielercharakteren: `Equ_RightHand`, `Equ_Body`, `Equ_Leg`, ...) - dieser komplexere Fall wird
NICHT unterstützt, NPCs damit bleiben bei der bisherigen 2D-Markierung. Gegen die Karte "RouN"
(29 NPCs, alle Shop-/Quest-Typen) geprüft: **29 von 29 haben ein direkt auflösbares, eigenes
Modell** (`NpcViewIndex==0`) - der komplexe Ausrüstungsfall scheint für handgefertigte,
benannte NPCs die Ausnahme zu sein, nicht die Regel. Gefundenes Modell wird in
`state.availableNifFiles` gesucht (dieselbe Liste, die auch der Objekt-Asset-Picker nutzt) -
kein Treffer heißt einfach "Datei nicht in der aktuell bekannten Nif-Bibliothek", kein Fehler.
3D-Modelle werden bei jeder Coord-X/Y/Richtung-Bearbeitung im NPC-Tab automatisch neu
positioniert (`npcRenderSetForMap`-Invalidierung).

**Mob-Spawn-Zonen im 2D-View:** kleines gefülltes Kreis-Icon in der Zonenmitte + dünner
Umriss-Kreis für den Spawn-Radius (Nutzerwunsch: "Icon in der Mitte...Markierung außen rum die
den Spawn-Radius zeigt"). Radius-Näherung, da die Zonen nicht immer echte Kreise sind: bei
`Width`/`Height` > 0 die größere Seite halbiert, sonst (häufigster Fall: beide 0) `RangeDegree`
direkt als Weltraum-Radius - trotz des Namens KEIN Winkel in diesen Fällen (Werte wie 507
passen nicht in 0-360°, siehe `MobRegenGroup` in docs/SHN_DEPENDENCIES.md-Nachbarschaft). Auch
Klick-Auswahl der Zonen im 2D-View ergänzt (etwas großzügigere Toleranz als bei NPCs, da Zonen
typischerweise ausgedehnter sind).

Nur syntaktisch geprüft (siehe [0.44.9]). Auflösungslogik (NPC-Name → Modelldatei) separat mit
einem eigenen Test-Programm gegen die echten NA2016-Daten verifiziert (siehe oben, 29/29
Treffer) - keine automatisierte Test-Suite dafür, da GL-Rendering selbst in dieser Sandbox
nicht visuell prüfbar ist. Keine Core-Änderung - alle 8 Test-Suiten unberührt.

**Bitte nach dem nächsten echten Build besonders sorgfältig visuell verifizieren:** NPC-
Modelle an der richtigen Position/Ausrichtung, keine doppelten Instanzen, Performance bei
vielen NPCs auf einer Karte (analog zum bereits bekannten NIF-Ladekosten-Hinweis aus [0.44.9]).

## [0.44.16] — NPC-Dialog-Editor (NpcDialogData.shn)

Letzter fehlender Baustein aus der ursprünglichen Anfrage (NPC-Platzierung/Mob-Spawn/Händler/
Dialog): neuer "Dialog bearbeiten"-Knopf im NPC-Tab (für jeden NPC, nicht nur Händler) öffnet
ein Popup mit Begrüßungstext (Mehrzeilen-Eingabe, `[NAME]`-Platzhalter bleibt erhalten) und
einer bearbeitbaren Button-Liste (Label + Aktion, hinzufügen/entfernen). Baut auf der schon
vorhandenen `ShnFile`-Infrastruktur auf - `NpcDialogData.shn`s variable-Länge-String-Spalte
(Typ 26) wurde bereits beim Schreiben korrekt unterstützt, keine Core-Änderung nötig.

Neuer Parser für das Dialog-Skript-Format selbst (`ParseNpcDialogText`/`SerializeNpcDialogText`
in `main.cpp`): `<Begrüßungstext>` gefolgt von `[BUTTON_NPC]=[Label][Aktion]`-Zeilen. Byte-exakt
gegen alle Einträge in der echten `NpcDialogData.shn` geprüft (223 Zeilen) - dabei einen echten
Randfall gefunden und korrigiert: manche Buttons haben nur EINE Klammer (nur Aktion, kein
Label, z.B. `RouGaianMaria`: `[BUTTON_NPC]=[server_ack 1]`) - vermutlich zeigt der Client dafür
einen generischen "Weiter"-Button. Nach dem Fix rundet der Inhalt (Begrüßung + alle Buttons)
bei allen 223 Einträgen exakt - die verbleibenden 14 Byte-Abweichungen sind nachweislich nur
ein einzelnes nachlaufendes Leerzeichen am Dateiende (Kopier-Artefakt in den Originaldaten,
inhaltlich bedeutungslos).

Client-Ordner (`ressystem`, enthält `NpcDialogData.shn`) wird automatisch unter dem Projekt-
Client-Ordner gesucht, mit manuellem Eingabefeld als Fallback (gleiches Muster wie
`shineTextRoot`). Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 8
Test-Suiten unberührt.

**Damit sind jetzt alle vier ursprünglich angefragten Teile umgesetzt:** NPC-Platzierung,
Mob-Spawn-Editor, Händler-Konfiguration, Dialog-Konfiguration. Weiterhin offen (kein Teil der
ursprünglichen Anfrage, aber naheliegende Ergänzungen): 3D-Marker/-Rendering für NPCs und
Händler (bisher nur 2D), visuelle Zonen-Darstellung für Mob-Spawns im 2D-View.

## [0.44.15] — Neues Textformat "ShineText" (World/NPC.txt, MobRegen, NPCItemList) + echte
    NPC-Platzierung, Mob-Spawn-Zonen und Händler-Inventar im Map-Editor

**Grundlegender Fund:** neben den binären `.shn`-Dateien gibt es unter `Server/9Data/Shine`
eine dritte, eigenständige Datenformat-Familie: reine Klartext-Tabellen (`#Table`/
`#ColumnType`/`#ColumnName`/`#record`). Das sind die tatsächlichen Quellen für NPC-Platzierung
(`World/NPC.txt`, 527 Einträge: Name/Karte/Coord-X/Coord-Y/Richtung/Rolle/Rollen-Argument),
Mob-Spawns (`MobRegen/<Karte>.txt`, zwei verknüpfte Tabellen: benannte Zonen + Zone→Monster+
Anzahl+Respawn-Zeitkurve) und Händler-Inventar (`NPCItemList/<NPC>.txt`, mehrere Tabs mit
Item-Slots). Der Datei-Kopf behauptet fälschlich "Leerzeichen ist Trennzeichen" - tatsächlich
ist es durchgehend TAB.

**Neuer Parser** `ShineText.hpp/cpp` (gleiche Architektur wie `ShnFile.cpp`, minimal-invasives
Speichern über `sourceLine`-Referenzen - unbekannte Zeilen/Kommentare/nicht-UTF8-Bytes wie
koreanische Kommentare bleiben unangetastet). Beim Testen gegen echte Daten drei eigene
Parser-Bugs gefunden und behoben, bevor sie in Gebrauch gegangen wären: (1) führendes Leer-Tab
vor manchen Direktiven wie in `World/NPC.txt`s `#Table`-Zeile, (2) ein zusätzliches Leer-Tab
nach `#ColumnType`/`#ColumnName`, das Spalten- und Wertlisten um eins verschob, (3) neue Records
müssen IMMER `#recordin <Tabellenname>` statt `#record` nutzen, da `#record` beim Neuladen der
zuletzt im Dateistrom deklarierten Tabelle zugerechnet wird, nicht zwingend der Zieltabelle
(`World/NPC.txt`s NPC-Einträge liegen z.B. weit hinter der letzten `#Table`-Zeile). Neue,
achte Test-Suite `test_shinetext.cpp` (Laden, Round-Trip, Bearbeiten, Anhängen) - gegen alle
126 `MobRegen`- und 72 `NPCItemList`-Dateien sowie 48 `World`-Dateien geprüft, nur `QuestParser.txt`
(offenbar eine echte Grammatik-Datei) und ein paar sehr breite Tabellen (`ItemDropTable.txt`
mit 270+ Spalten, `MiscDataTable.txt`, `Scenario.txt`, `SubLayerInteract.txt`, `Quest.txt`)
haben noch nicht behobene Kanten - für die aktuelle Aufgabe (NPC/Mob/Händler) nicht gebraucht.

**Map-Editor: NPC- und Mob-Spawn-Tabs jetzt echt implementiert** (`EditMode::Npcs`/`Mobs` waren
bisher reine "noch nicht implementiert"-Platzhalter aus einer früheren Sitzung). Kartenname
zur Filterung kommt aus `state.legacySaveStem` (beim Öffnen/Anlegen einer Karte gesetzt) -
gegen echte Daten geprüft: "Rou" und "RouN" sind tatsächlich zwei verschiedene Karten (Rou hat
nur 4 NPCs, RouN die Läden/Händler) - keine Normalisierung nötig, Map-Spalte entspricht 1:1
echten Kartennamen. NPC-Tab: Liste + Position/Richtung bearbeiten + 2D-Marker (Quadrate, um
sich von den runden Objekt-Markern zu unterscheiden, dieselbe Weiß/Blau-Logik) + Klick-Auswahl
im 2D-View + "Händler-Inventar bearbeiten"-Knopf bei `Role=Merchant` (öffnet Popup mit
`NPCItemList/<NPC>.txt`, direkt bearbeitbar). Mob-Tab: Spawn-Zonen-Liste + Position/Größe
bearbeiten + Anzeige der verknüpften Monster. Beide speichern minimal-invasiv zurück.

**Noch NICHT umgesetzt** (nächster Schritt): Dialog-Editor für `NpcDialogData.shn`s
`[BUTTON_NPC]=[Label][Aktion]`-Skriptformat, 3D-Marker/-Rendering für NPCs (bisher nur 2D),
Mob-Spawn-Zonen-Marker im 2D-View (bisher nur Liste, keine visuelle Darstellung der Zonen).

Nur syntaktisch geprüft (siehe [0.44.9]). Core-Änderung (neues Modul) - alle 8 Test-Suiten
grün, inkl. der neuen `test_shinetext`.

## [0.44.14] — SHN-Editor: Namensstamm-Abhängigkeitserkennung + Grün/Rot-Feldmarkierung mit
    Propagation + echte XP-/Preis-Editoren

**Namensstamm-Abhängigkeitserkennung** (`ShnNameStem`, `FindNameStemPeers`): trennt bekannte
Suffixe ab (`InfoServer`, `ViewInfo`, `Info`, `View`, `Server`, `Desc`, `Group`, `Data`, `List`,
`Rate`, `Count`, `Type`, `State`) und gruppiert nach übereinstimmendem Rest - deutlich
zuverlässiger als reine Zeilenanzahl (`FindRowCountPeers` aus [0.44.13]), da auch Teilmengen-
Beziehungen erkannt werden (z.B. `ItemShopView.shn` mit 3559 Zeilen als Teilmenge von
`ItemShop.shn` mit 3930 Zeilen - die Zeilenzahl stimmt hier bewusst NICHT überein).
`FindDependencyPeers` vereint beide Signale und ersetzt `FindRowCountPeers` im Datei-Browser.
Vollständige, gegen alle 350 echten NA2016-SHN-Dateien (Client+Server) verifizierte Ergebnisse
in `docs/SHN_DEPENDENCIES.md` - u.a. drei bestätigte "Xxx/XxxInfoServer/XxxView"-Familien
(Item: 14999 Zeilen, Mob: 2878, ActiveSkill: 2791) plus ~15 weitere Namens-Paare.

**Grün/Rot-Feldmarkierung mit Propagation** (Mockup-Wunsch): neues Feld
`EditorState::ShnDocument::cellStatus` (pro Zelle: normal/grün="automatisch übernommen"/
rot="braucht Eingabe") - rein Editor-seitiger Sitzungs-Zustand, nicht Teil des .shn-Formats.
Neuer Button **"+ Neue Zeile (in Familie propagieren)"** im Single-SHN-Editor
(`AddRowWithPropagation`): legt eine neue Zeile in der aktuellen Datei an und - für jedes
gerade geladene, per `FindDependencyPeers` erkannte Familienmitglied - ebenfalls eine neue
Zeile, wobei gleichnamige Spalten aus der Quellzeile übernommen (grün) und alle übrigen rot
markiert werden. Zellen verlieren ihre Markierung automatisch, sobald der Nutzer sie manuell
bearbeitet (`DrawShnCellEditor`). Grid (`DrawShnGrid`) färbt die Zellhintergründe entsprechend.

**Echte XP-/Preis-Editoren** (vorher nur Platzhaltertext mit Verweis auf die Multi-SHN-Ansicht):
`DrawScalableFieldEditor` + `FindOrLoadShnDoc` (lädt die Zieldatei automatisch aus dem bereits
bekannten SHN-Ordner nach, falls noch nicht offen). **XP Rate Editor** zeigt `MobInfoServer.shn`
(ID/InxName/`MonEXP`/`EXPRange`) mit einer "alle Werte um X% skalieren"-Aktion - eine echte
Recherche ergab, dass es KEINE zentrale Level→EXP-Kurventabelle gibt, EXP wird stattdessen pro
Monster in `MobInfoServer.MonEXP` vergeben, daher dort angesetzt. **Buy & Sell Editor** zeigt
`ItemInfo.shn` (ID/InxName/`BuyPrice`/`SellPrice`) mit derselben Skalier-Aktion - `ItemShop.shn`/
`ItemShopView.shn` wurden geprüft, enthalten aber keine Preisfelder (nur Sortiment/Anzeige),
der Preis selbst sitzt direkt in `ItemInfo`.

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 7 Test-Suiten unberührt.

## [0.44.13] — SHN-Editor: echte Abhängigkeits-Analyse (aus allen 350 NA2016-SHN-Dateien) +
    Datei-Browser-Warnmarkierung + Dropdown für kategorische Felder

**Analyse zuerst (gegen alle 350 echten SHN-Dateien aus NA2016.zip, Client+Server):** Ein
Werte-Überlappungs-Test gegen `ItemInfo.shn`s ID-Spalte (14999 Zeilen, IDs 0-65504) erzeugte
zunächst über 500 falsche Positive - bei einer so dichten ID-Menge trifft praktisch jede
kleine Zufallszahl (Stat-Werte, Farbwerte, Prozentsätze) "zufällig" eine gültige ID. Reine
Statistik ist für diesen Datensatz also NICHT verlässlich. Stattdessen zwei robustere Signale
verwendet: (1) **Zeilenanzahl-Gleichschritt** - `ItemInfo.shn`, `ItemInfoServer.shn` UND
`ItemViewInfo.shn` haben alle exakt 14999 Zeilen, ein starkes, verifiziertes Indiz für eine
echte 1:1-Abhängigkeit; (2) **Spaltennamen-Analyse** - über 30 Dateien mit explizit
item-bezogenen Spaltennamen gefunden (`ItemIDX`, `ItemID`, `Item_Inx`, `SetItemIndex`,
`ItemActionID` usw., meist als String-Referenz auf den Item-internen Namen, nicht die
numerische ID). Vollständige Liste nur diese Sitzung ermittelt, nicht dauerhaft im Repo
gespeichert - siehe Chat-Antwort. **Wichtig:** eine vollständige, für alle 350 Dateien
verifizierte Abhängigkeitsmatrix ist ein eigenes, mehrsitzungs-großes Vorhaben (vergleichbar
zur NIF-Parser-Arbeit) - hier nur der Anfang.

**Datei-Browser-Warnmarkierung (Single-SHN-Editor, Client- UND Server-Liste):** jede Datei mit
mehr als 20 Zeilen, deren Zeilenanzahl exakt mit einer anderen geladenen Datei übereinstimmt,
bekommt jetzt ein gelbes ⚠-Symbol davor plus Tooltip mit den betroffenen Dateien
(`FindRowCountPeers`). Rein aus den geladenen Daten abgeleitet, keine Dateinamen-Heuristik.

**Dropdown für kategorische Felder** (`DrawShnCellEditor`): wenn eine ganzzahlige Spalte über
das GESAMTE Dokument hinweg nur 2 bis 24 unterschiedliche Werte annimmt (z.B. `ItemInfo.Class`,
`UseClass`, `Type`, `Equip`, `WeaponType`), erscheint zusätzlich zum Freitext-Feld ein Dropdown
mit den tatsächlich beobachteten Werten. Bewusst KEINE erfundenen Klartext-Bezeichnungen (z.B.
welche Zahl "Schwert" bedeutet) - nur die reinen Zahlencodes, da deren semantische Bedeutung
nicht dokumentiert/bekannt ist.

**Noch NICHT umgesetzt** (Mockup-Wunsch, siehe Chat): grün/rot-Feldmarkierung je nachdem, ob
ein Feld aus einer anderen Datei automatisch eingefügt wurde oder noch offen ist (setzt eine
Änderungs-Tracking-Infrastruktur voraus, die es noch nicht gibt), sowie eigenständige
XP-/Preis-Editoren (bisher nur Platzhalter-Text, verweist auf Multi-SHN-Kandidatenansicht).

Nur syntaktisch geprüft (siehe [0.44.9]). Keine Core-Änderung - alle 7 Test-Suiten unberührt.

## [0.44.12] — Theme auf Schwarz/Blau/Grau/Weiß + Map-Editor-Arbeitsfläche als echtes DockSpace

**Theme:** die 6 bunten Kachelfarben auf dem Projekt-Hub (grün/oliv/gold/orange/braun/rot)
durch abgestufte Blau-/Grautöne ersetzt. Die rot/gelben Objekt-Marker (2D-Ansicht UND die
3D-Platzhalter-Pyramiden, `ObjectMarkerRenderer`s Fragment-Shader) auf Weiß (ausgewählt) /
Blau (übrige) umgestellt - konsistent zwischen 2D und 3D. Bewusst NICHT angefasst: die gelbe
"prüfen/ggf. ändern"-Markierung im SHN-Mehrfachprofil-Abgleich (`ShnSourceColor`/Zeile ~1611 -
funktionale Warnfarbe, kein reines Chrome) und der grün-graue Höhen-Gradient der Heightmap-
Vorschau (`Renderer.cpp`, Tiefland=grün/Bergspitzen=hellgrau - Inhalts-, keine Theme-Farbe).
Der Rest der App (`ApplyEditorTheme`) war bereits durchgehend Schwarz/Blau/Grau/Weiß.

**Verschiebbare/skalierbare Panels:** die Map-Editor-Arbeitsfläche (Werkzeuge/2D/3D) nutzt
jetzt ein echtes ImGui-DockSpace statt dreier fest breiter `BeginChild`-Spalten - alle drei
Panels sind jetzt per Ziehen an den Rändern skalierbar UND per Ziehen am Tab frei verschiebbar/
neu anordenbar/stapelbar (`DrawMapEditorWorkspace`, `imgui_internal.h`/`DockBuilder*` für das
programmatische Default-Layout beim allerersten Betreten - Tools 22% links, 2D/3D 50/50 im
Rest, identisch zum bisherigen festen Layout). Spätere Nutzer-Anpassungen bleiben für den Rest
der Sitzung erhalten. **Wichtig:** dies betrifft NUR die Arbeitsfläche selbst - der äußere
App-Rahmen (Projekt-Hub, Bildschirm-Navigation) bleibt bewusst das feste, mockup-basierte
Layout aus einer früheren Sitzung (siehe Kommentar bei `##MainHost` in `main()`) und wurde
NICHT zurückgebaut. `imgui_internal.h` ist kein Teil der öffentlichen ImGui-API, aber in
Docking-Branch-Apps der übliche Weg für ein solches Default-Layout - sollte über vcpkgs
imgui-Paket wie gewohnt mitkommen, aber nach dem nächsten echten Build bitte gezielt prüfen.

Nur syntaktisch geprüft (gegen die echten Dear-ImGui-Docking-Header inkl. `imgui_internal.h`,
frisch von GitHub, plus GLFW-Header und glad-Stub, siehe [0.44.9]) - keine Core-Änderung, alle
Core-Test-Suiten unberührt. **Bitte nach dem nächsten Windows-Build besonders sorgfältig
visuell verifizieren** - DockSpace-Verhalten lässt sich ohne echten GL-Kontext nicht testen.

## [0.44.11] — `.nif`: `NiPalette` hat kein "Num Entries"-Feld (+5 Dateien) + systematische
    Mehrfach-Hop-Ursachenanalyse aller 811 verbleibenden Fehlschläge

**Analyse zuerst (kein Code):** Die von HANDOFF.md seit Längerem vorgeschlagene, aber nie
durchgeführte vollständige Mehrfach-Hop-Rückverfolgung aller Fehlschläge wurde diese Sitzung
gemacht (Werkzeug bindet `NifModel.cpp` direkt ein, um an die interne Blocktyp-Tabelle im
Header zu kommen, ohne die Datei selbst zu ändern). Ergebnis: **646 von 811 (80%)** haben ein
`NiTriStrips(Data)`/`NiTriShape(Data)`- oder `NiMaterialProperty`-Vorkommen unter den letzten
3 Blöcken vor dem Fehlschlag - also mit sehr hoher Wahrscheinlichkeit Folgefehler der ZWEI
TABUISIERTEN Funktionen (`ParseNiTriStripsHeader`, `NiMaterialProperty`s 14-vs-15-Float-Frage,
siehe HANDOFF.md "Arbeitsweise"). Diese wurden NICHT angefasst. Verbleiben **163 Dateien ohne
erkennbare Nähe zu den Tabu-Funktionen** - das ist der tatsächlich noch bearbeitbare Rest,
nicht alle 811. Ein naheliegender Kandidat aus diesen 163 (Partikelsystem-Modifier-Kette,
`NiPSysSpawnModifier`/`NiPSysBoxEmitter`/`NiPSysMeshEmitter`, ~65 Dateien) führte zurück zum
bereits dokumentierten Dead End "Unknown QQSpeed Floats" (docs/MAP_FORMAT.md, Dead End 2) -
NICHT erneut versucht, da per vollem Massentest bereits als falsch bewiesen (2618→2594).

**Echter Fix:** `NiPalette` hat in diesem Fork KEIN "Num Entries"(u32)-Feld, anders als die
autoritative nif.xml-Referenz - direkt nach `has_alpha`(u8) folgen IMMER exakt 256
`ByteColor4`-Einträge (1024 Byte). Byte-exakt an 2 unabhängigen Dateien verifiziert
(`filddoll.nif`, `Sign01.nif`, unterschiedliche Größe/Inhalt): in BEIDEN endet eine lange
Nullen-Sequenz exakt bei `has_alpha`+1024 Byte, danach beginnt sofort ein neues, plausibles
Blockmuster (bei `Sign01.nif` z.B. eine klar erkennbare Graustufen-Farbtabelle, Muster
`xx xx xx FF` je Eintrag). Massentest: **2625 → 2630/3436 (76,5%)**, keine Regression (alle 8
Test-Suiten weiterhin 0 Fehler). Die von der Referenz vorgesehene "kann auch 16 Einträge
sein"-Variante wurde an diesen 2 Belegen nicht beobachtet.

**Nächste Kandidaten aus den 163** (nach Häufigkeit, siehe HANDOFF.md): `NiNode` (43),
`NiBillboardNode` (9), `NiTexturingProperty` (3), `NiStringExtraData` (3), `NiSourceTexture`
(3), `NiTransformData` (2), `NiFloatData` (2) - noch nicht untersucht.

## [0.44.10] — Einmalige NIF-Vorschaubild-Vorladung beim Öffnen des Map-Editors +
    Impact-Analyse der noch scheiternden `.nif`-Dateien

**Vorlade-Sequenz:** `DrawMapEditorLauncher` stößt jetzt, sobald ein `resmap`-Ordner gefunden
wird (derselbe Scan wie für die Kartenliste, kein Zusatzaufwand), einmalig eine Vorladung ALLER
Vorschaubilder der `.nif`-Bibliothek an (`StartNifThumbnailPrecache`/`AdvanceNifPrecache`) -
nicht nur der auf einer bestimmten Karte platzierten Objekte, sondern der gesamten unter
`nif`/`nifs` gefundenen Dateiliste (identisch zu der, die der NIF-Asset-Picker selbst
durchsucht). Verarbeitet 8 Dateien pro Frame (nicht alle auf einmal) mit sichtbarem
Fortschrittsbalken statt eines einzelnen langen Frames, der wie ein Absturz aussähe. Läuft
GENAU EINMAL pro `resmap`-Ordner pro Sitzung (Wiederholung bei erneutem Öffnen desselben
Projekts wird übersprungen, `EditorState::nifPrecacheDoneForRoot`). Bei der Referenz-
Bibliothek dieser Sitzung (3436 Dateien, ~4ms/Datei im Schnitt) macht das ca. 14 Sekunden
einmalig beim ersten Öffnen des Map-Editors - danach ist der NIF-Asset-Picker beim Scrollen
durchgehend ruckelfrei (kein Erst-Zugriff-Ruckler mehr, siehe [0.44.9]). Reine main.cpp-
Änderung, keine Core-Änderung nötig. Zwei Vorwärtsdeklarationen ergänzt (einzige bewusste
Ausnahme vom sonst in main.cpp durchgehend eingehaltenen Top-Down-Stil) - die Thumbnail-
Infrastruktur aus [0.44.9] liegt textuell nach `DrawMapEditorLauncher`. Nur syntaktisch
geprüft (siehe [0.44.9] für die Prüfmethode), keine Warnungen.

**Impact-Analyse der 811 noch scheiternden `.nif`-Dateien:** Nutzerfrage, ob sich weitere
Parser-Fixes noch lohnen. Zwei eigene Analyse-Tools gebaut (nicht Teil des Repos, nur für
diese Sitzung) und gegen alle 218 lesbaren `.shmd`-Objekt-Placement-Dateien (214 echte Karten,
221.401 platzierte Objekte) aus den 5 `resmap`-Archiven laufen lassen, mit exakt derselben
Pfad-Auflösung wie die echte App (`ResolveLegacyAssetPath`, pro Karte). Ergebnis: von 77.107
in diesem (unvollständigen Sandbox-)Datensatz auflösbaren Platzierungen sind 6.664 (8,6%)
von einem aktuell kaputten Parser betroffen (242 eindeutige Datei-Pfade) - der Rest der
144.294 Platzierungen war schlicht nicht in den 5 Archiven enthalten (unvollständiger
Testdatensatz, keine Aussage über den echten Client). Wichtig: die Betroffenheit ist NICHT
gleichmäßig verteilt - wenige Modelle dominieren durch massenhafte Platzierung, allen voran
`grass.nif` (603 Platzierungen allein auf der Karte TevaL), `sin_firelamp.nif` (346),
`light-tru.nif` (316), die Tunnel01/02-Holzsegmente (zusammen 761), `woodbridge.nif` auf
KDPrtShip (214) - die Top 10 Dateien allein stellen etwa die Hälfte aller 6.664 betroffenen
Platzierungen. `streetlight1.nif` scheitert dabei an ALLEN 5 im Datensatz vorhandenen Kopien
gleich (systemischer Fehler an Block 52/NiTriStrips, keine Dateibesonderheit). Fehlerursachen
bleiben aber weiterhin ein echter langer Schwanz unterschiedlichster Block-EOF-Fehler (siehe
HANDOFF.md) - kein einzelner Fix würde einen Großteil lösen. **Empfehlung:** nicht pauschal
alle 811 Dateien gleich behandeln, sondern gezielt nach Platzierungs-Häufigkeit priorisieren,
beginnend mit `grass.nif` (Block 5/NiTriStrips EOF). Kein Code geändert, nur Analyse - siehe
Antwort im Chat für die vollständige Zahlen- und Dateiliste.

## [0.44.9] — Vorschaubilder im Asset-Picker + Icons in der Textur-Layer-Liste

**Vorschaubilder:** `DrawAssetPickerPopup` (Textur- und NIF-Auswahl) zeigt jetzt ein kleines
Vorschaubild vor jedem Dateinamen. Für Texturen direkt über `LoadDdsImage`; für `.nif`-Modelle
die erste gefundene Diffuse-Textur des Modells (eingebettet über `NiPixelData` ODER extern
über den `fieldTexture`-Ordner aufgelöst, siehe `ResolveLegacyAssetPath`). Neuer, gecachter
Vorschaubild-Speicher (`EditorState::assetThumbnails`, Schlüssel: aufgelöster Pfad) - ein
Eintrag ohne Vorschaubild (z.B. Mesh ohne Textur) wird ebenfalls gecacht, damit er nicht bei
jedem einzelnen Frame neu geparst wird.

**Performance:** die Datei-Liste wird jetzt zuerst gefiltert (reine String-Vergleiche) und erst
danach per `ImGuiListClipper` gezeichnet - Vorschaubilder werden dadurch NUR für die gerade
sichtbaren Zeilen geladen, nicht die komplette (ggf. tausende Einträge lange) Liste auf einen
Schlag. Ein `.nif`-Vorschaubild kostet beim ersten Sichtbarwerden einen vollen `LoadNifMesh`-
Aufruf (~4ms im Schnitt laut Massentest dieser Sitzung, siehe [0.44.8] unten) - bei sehr
schnellem Scrollen durch viele neue Einträge ggf. ein kurzer, einmaliger Ruckler pro Zeile,
danach dauerhaft gecacht.

**Layer-Liste:** die Textur-Layer-Liste im Texturing-Panel zeigt jetzt ein 20x20-Icon pro Layer
(aus `TextureLayer::diffuseFileName`, über denselben Cache). Icons erscheinen erst, nachdem der
Textur-Picker in der laufenden Sitzung mindestens einmal geöffnet wurde (sonst ist
`textureAssetRoot` leer) - kein Fehlerfall, nur kein Icon.

GL-Texturen des Caches werden beim App-Shutdown wieder freigegeben.

Diesmal syntaktisch geprüft gegen die ECHTEN Dear-ImGui-Docking-Header + echten GLFW-Header
(beide frisch von GitHub, nicht nur ein Minimal-Stub) + einen neu geschriebenen `glad.h`-Stub
mit den tatsächlich im Projekt genutzten GL-Funktionssignaturen - `-fsyntax-only` über alle
`src/app/*.cpp` sauber, keine Warnungen (u.a. einen `-Wdangling-reference`-Fund von GCC 13
unterwegs behoben - Fehlalarm, aber Aufrufstellen trotzdem auf benannte lokale Variablen
umgestellt, `GetOrLoadAssetThumbnail` gibt jetzt außerdem per Wert statt per Referenz zurück).
Kein echter GL-Renderer in der Sandbox - bitte nach dem nächsten Build visuell verifizieren
(Icons sichtbar, korrekte Bilder, kein spürbares Ruckeln beim Scrollen).

## [0.44.8] — Eingebettete NIF-Texturen werden dekodiert und gerendert (vom Nutzer selbst
    umgesetzt, diese Sitzung nachträglich verifiziert)

Diese Änderung war beim Start dieser Chat-Sitzung bereits im Code vorhanden (vom Nutzer selbst
zwischen den Sitzungen umgesetzt), aber weder hier noch in HANDOFF.md dokumentiert. Von Claude
diese Sitzung per echtem g++-Build + Massentest nachträglich geprüft und hier nachgetragen.

**Was geändert wurde:** `ParseNiSourceTexture` liefert jetzt eine vollständige Struktur
(`NifTextureSource{filename, useExternal, pixelDataRef}`) statt nur eines Dateinamens. Neue
Funktion `ParseNiPixelData` liest die eingebettete `NiPixelData`-Textur (Header, Mipmap-Kette,
Rohpixel) wirklich ein und dekodiert sie über `DecodeBcImage` (dieselbe Funktion, die auch für
externe `.dds`-Dateien genutzt wird) - vorher wurde dieser Block nur strukturell übersprungen
(`SkipNiPixelData`). **Die tabuisierte `SkipNiPixelData` wurde dabei NICHT verändert** (siehe
HANDOFF.md, "zwei frühere Katastrophen-Regressionen") - sie ist jetzt nur toter Code, denn
`ParseNiPixelData` dupliziert ihre exakte Byte-Konsum-Logik (72/50-Byte-Header je nach
NIF-Version, gleiche Mipmap-Schleife, gleiches `CountU32`-Limit) und liest zusätzlich statt nur
zu überspringen. `NifMeshRenderer` lädt eingebettete Texturen jetzt per `glTexImage2D` hoch
(`GetOrLoadEmbeddedTexture`, analog zur bestehenden `GetOrLoadTexture` für externe Dateien).

**Verifiziert diese Sitzung (echter g++-Build, kein `-fsyntax-only`):**
- Alle 8 Test-Suiten gegen echte Rou.*-Referenzdaten: 0 Fehler.
- Selbst gebauter Massentest (kein fertiges Skript im Repo vorhanden) über alle 3436 echten
  `.nif`-Dateien aus den 5 `resmap`-Archiven: **2625/3436 (76,4%)** ladbar - exakt der vom
  Nutzer genannte Wert, also stabil seit der letzten Messung.
- Eingebettete Texturen: **4935 von 4935** Dekodier-Versuchen erfolgreich, bis auf
  Pixelformat 3 (unkomprimiert/palettiert, ~12 Dateien, z.B. `wall_4.png`, `floor_17.png`) -
  `DecodeBcImage` unterstützt bisher nur Format 4/5/6 (DXT1/3/5), siehe "Was NICHT
  funktioniert" in HANDOFF.md.

Weiterhin KEIN echter Build/visueller Test der Objekt-Texturierung selbst (siehe HANDOFF.md).

## [0.44.7] — Datei-Picker für Texturen/NIF-Modelle + feinerer Block/Walk-Pinsel

**Textur- und Modell-Auswahl:** Neuer "Durchsuchen..."-Knopf neben dem Diffuse-Feld
(Texturing-Panel) und dem Modellpfad-Feld (Object-Placement-Panel) - öffnet ein filterbares
Auswahl-Popup statt manueller Pfadeingabe. Sucht automatisch im `fieldTexture`- bzw.
`nif`/`nifs`-Ordner unterhalb des per Client-Ordner gefundenen `resmap` (inkl. Unterordner,
begrenzt rekursiv). Dateilisten werden beim Öffnen des Popups einmalig gescannt und gecacht
(nicht pro Frame - bei tausenden `.nif`-Dateien spürbar teuer). Mit simulierten Testdaten
verifiziert (verschachtelte Unterordner, `nif` singular wie vom Nutzer angegeben).

**Feinerer Block/Walk-Pinsel:** Radius-Schieberegler von 10-1000 auf 1-1000 abgesenkt, jetzt
logarithmisch skaliert (kleine Werte lassen sich dadurch deutlich präziser einstellen als bei
linearer Skalierung über einen so großen Bereich). Neuer "1 Zelle"-Knopf setzt den Radius
exakt auf die halbe Diagonale einer Walk-Gitterzelle - trifft garantiert nur die
nächstgelegene Zelle, unabhängig von der (nicht quadratischen) Zellgröße.

Nur syntaktisch geprüft bzw. an simulierten Daten getestet (kein echter GL-Renderer in dieser
Sandbox) - bitte nach dem nächsten Build Rückmeldung geben.

## [0.44.6] — Zwei echte Renderfehler behoben: Diffuse-Textur-Flip zurückgenommen +
    NIF-Objekt-Texturen finden jetzt den "fieldTexture"-Ordner

**Map-Textur oben/unten vertauscht:** Nutzer-Vergleich zweier Screenshots (2D View mit
rotem Block/Walk-Overlay vs. Nahaufnahme derselben Steintextur) zeigte: das Block/Walk-
Overlay ist korrekt ausgerichtet, aber die Diffuse-Textur weiterhin vertauscht - trotz des
in einer früheren Sitzung eingeführten V-Flips. Da Block/Walk dieselbe `mapUv`-Achse wie
Blend nutzt (beide korrekt), war dieser Flip die falsche Richtung. Zurückgenommen
(`Renderer.cpp`, `SampleLayer`): Diffuse-UV nutzt jetzt `mapUv` direkt, ohne V-Flip, wie
Blend/Block/Walk auch. DDS-Decoder selbst geprüft und für korrekt befunden (liest Zeilen
originalgetreu top-down, kein Flip nötig) - die Ursache lag ausschließlich in der
UV-Zuordnung.

**NIF-Objekte ohne Textur:** `.ini`-Pfade (Heightmap, Textur-Set) enthalten immer den vollen
`resmap\field\<Karte>\...`-Pfad, aber Objekt-Texturen aus `.nif`-Dateien sind oft NUR ein
nackter Dateiname ohne Verzeichnisangabe (z.B. `"ELDERIN_wg.DDS"`, byte-exakt bestätigt).
`ResolveLegacyAssetPath` sucht bei solchen Ein-Komponenten-Pfaden jetzt zusätzlich gezielt im
`fieldTexture`-Ordner (auch in dessen Unterordnern, begrenzt rekursiv). Zusätzlich toleriert:
manche Original-Dateinamen haben ein Leerzeichen MITTEN im Namen vor der Endung (z.B.
`"road01_lamp .dds"`) - wird jetzt beim Vergleich ignoriert. Mit simulierten Testdaten
verifiziert (echte `.dds`-Dateien in dieser Sandbox nicht vorhanden).

Beide Fixes nur syntaktisch geprüft bzw. an simulierten Daten getestet (kein echter
GL-Renderer in dieser Sandbox) - bitte nach dem nächsten Build erneut Rückmeldung geben.

## [0.44.5] — Mehrere gleichnamige "resmap"-Ordner möglich - wählt jetzt den mit echten
    Kartendaten statt blind den ersten Treffer

Rückmeldung: die Suche zeigte "Client/reschar/resmap" an, was es laut Nutzer nicht gibt bzw.
keine echten Karten enthält. Ursache: es kann MEHRERE Ordner namens "resmap" geben (z.B.
einen leeren/unbenutzten an anderer Stelle im Client-Baum) - die bisherige Suche
(`FindResmapFolder`) stoppte beim ERSTEN Treffer, unabhängig davon, ob er echte Kartendaten
enthielt.

Neu: `FindAllResmapCandidates` sammelt JETZT ALLE Ordner namens "resmap" (bis zu drei Ebenen
tief), `ResolveMapSearchRootAndScan` scannt JEDEN Kandidaten und wählt den mit den MEISTEN
gefundenen Karten. Mit echten Testdaten verifiziert (nicht nur `-fsyntax-only`): ein
künstlicher leerer "reschar/resmap"-Ordner neben dem echten, 116 Karten enthaltenden
"resmap"-Ordner wurde korrekt zugunsten des echten übergangen. Bei mehreren Kandidaten zeigt
die Oberfläche jetzt zusätzlich an, wie viele gefunden wurden.

## [0.44.4] — Kartensuche gegen echte Referenzdaten verifiziert (nicht nur syntaktisch
    geprüft) + strengerer .ini-Filter

Die Scan-Logik (`FindResmapFolder`/`ResolveMapSearchRoot`/`ScanForMaps`) braucht kein
GLFW/ImGui/Windows - reines `std::filesystem`. Deshalb erstmals ECHT kompiliert und gegen
116 reale Kartenverzeichnisse (`resmap/field/<Map>/<Map>.ini`, `resmap/IDField/<Map>/<Map>.ini`
aus einem früheren Sitzungs-Korpus) laufen lassen, nicht nur `-fsyntax-only` geprüft. Ergebnis:
alle 116 Karten korrekt gefunden, über beide Kategorie-Ordner hinweg.

Zusätzlich verschärft: `ScanForMaps` akzeptiert jetzt nur noch `.ini`-Dateien, deren Name
(ohne Endung) exakt mit dem Namen ihres eigenen Ordners übereinstimmt
(`<MAPORDNER>/<MAPORDNER>.ini`) - schließt versehentliche Treffer bei anderen,
nicht zu einer Karte gehörenden `.ini`-Dateien aus. Karten-Liste zeigt beim Überfahren mit
der Maus jetzt den vollen gefundenen Pfad als Tooltip (Diagnose ohne Karte öffnen zu müssen).

## [0.44.3] — Kartensuche fand fälschlich "ressystem" statt "resmap" - jetzt gezielte,
    mehrstufige Suche ohne Fallback auf den ganzen Client-Ordner

Rückmeldung mit Screenshot: "resmap" lag beim Nutzer nicht direkt im gewählten Client-Ordner,
wodurch die bisherige Logik (v0.44.2) stillschweigend auf eine Volltextsuche im kompletten
Client-Baum zurückfiel und dabei zwei `.ini`-Dateien aus fremden "ressystem"-Ordnern fand
(falsche Treffer, keine echten Karten).

Neu: `FindResmapFolder` sucht gezielt nach einem Ordner namens "resmap" (Groß-/Kleinschreibung
egal) - direktes Kind des Client-Ordners zuerst, sonst bis zu zwei Ebenen tiefer (deckt
Distributionen ab, bei denen "resmap" in einem Unterordner liegt). Steigt dabei bewusst NICHT
in "ressystem" oder "fieldTexture" ab. `ResolveMapSearchRoot` gibt jetzt `std::optional` zurück
und liefert `std::nullopt`, wenn kein "resmap" gefunden wurde - **kein Fallback auf den
gesamten Client-Ordner mehr**. Die Karten-Browse-Ansicht zeigt bei Nichtfund jetzt klar
"'resmap' nicht gefunden" statt falscher Treffer.

## [0.44.2] — Erste echte Rückmeldung nach Build: Panel-Umschaltung + Ordnerdialog/Kartensuche
    repariert

Nach dem ersten tatsächlichen Build/Start (danke für die Rückmeldung!) zwei konkrete Bugs
behoben:

1. **"New Map"/"Map Öffnen" taten nichts.** Beide Panels waren immer gleichzeitig sichtbar,
   die Knöpfe hatten keinen Klick-Handler. Jetzt schaltet `state.mapLauncherView` (neu in
   `EditorState`) zwischen den beiden Panels um, die Knöpfe sind aktiv hervorgehoben.
2. **Karten unter `<Client>/resmap/...` wurden nicht gefunden, Ordnerdialog zeigte den
   Inhalt nicht zuverlässig an.** Zwei Ursachen behoben:
   - `BrowseForFolderWindows` nutzte die veraltete `SHBrowseForFolder`-API (bekannt für
     unzuverlässige Inhaltsanzeige/Navigation) - ersetzt durch die moderne
     `IFileOpenDialog`-COM-API (echtes Explorer-Fenster), jetzt zusätzlich als Kind-Fenster
     des Hauptfensters verankert (`glfwGetWin32Window`), damit er nicht dahinter verschwinden
     kann.
   - NEU `ResolveMapSearchRoot`: "Client Ordner" ist der Client-WURZELordner (enthält
     `resmap`, nicht direkt die Karten) - wird jetzt automatisch erkannt und durchsucht.
     Rescan-Trigger repariert (vorher: nur beim allerersten Aufruf, ein leeres Ergebnis löste
     nie einen erneuten Scan nach Ordnerwechsel aus). "Browse Map's" zeigt jetzt den
     tatsächlich durchsuchten Pfad + Trefferzahl an und hat einen "Neu durchsuchen"-Knopf,
     statt bei 0 Treffern rätselhaft leer zu bleiben.

**Weiterhin nur syntaktisch geprüft** (Linux-Sandbox, kein Windows verfügbar) - die
`IFileOpenDialog`-Änderung liegt komplett hinter `#ifdef _WIN32` und konnte hier NICHT
kompiliert werden (nur sorgfältig gegen die bekannte COM-API-Signatur geprüft). Bitte nach
dem nächsten Build erneut Rückmeldung geben.

## [0.44.1] — Korrektur: Klebezettel-Notizen aus den Mockups waren Umsetzungs-Hinweise, keine
    UI-Elemente

Auf Rückfrage klargestellt: die gelben Notizzettel in den Mockup-Bildern ("Noch nicht
entschieden", Projekt-Ordner-Erklärung, Dateiformat-Hinweis, Tools-Spalten-Hinweis) waren als
Kontext für die Umsetzung gedacht, nicht als sichtbare Bestandteile der fertigen Oberfläche.
Wieder entfernt aus `DrawEditorCard` (Karten-Notiz-Box), `DrawNewProjectConfig`,
`DrawMapEditorLauncher` und `DrawMapEditorWorkspace`. Die dahinterliegende Bedeutung bleibt
funktional erhalten (z.B. zeigt die Tools-Spalte weiterhin je nach Tab unterschiedliche
Werkzeuge). Tote Übersetzungsschlüssel in `Localization.hpp` entsprechend entfernt.

## [0.44.0] — GUI-Umbau nach Mockup-Vorgabe ("NextGen-Editor"): neue mehrstufige Navigation
    (Projekt-Hub, Projekt-Konfiguration, Map-Editor-Start, Arbeitsbereich) + Lokalisierung

Komplette Neustrukturierung der Oberfläche nach zwei vom Nutzer bereitgestellten Mockups.
Ersetzt das bisherige, immer sichtbare Andock-Fenster-Layout durch einen Bildschirm-
Zustandsautomaten (`AppScreen`) mit vier Stufen:

- **Projekt-Hub**: 6 Editor-Karten (MapEditor, SHN Editor, Quest Editor, Interface Editor,
  Drop Table, Skill+Action) mit vektoriellen Icons, Feature-Listen, Notizzetteln. Nur
  MapEditor ist funktional, der Rest führt zu einem "Noch nicht implementiert"-Platzhalter.
- **Neues Projekt konfigurieren**: Projekt Name/Ordner, Client-/Server-Ordner, speichert
  eine `project.tsproj`-Konfigurationsdatei.
- **Map-Editor-Start**: "Create New Map" (freie Maße statt fixem 257x257) + "Browse Map's"
  (sucht jetzt im Projekt-Client-Ordner).
- **Map-Editor-Arbeitsbereich**: Tab-Leiste (Hightmap/Texturing/Block-Walk/Objects/NPCs/
  NPC AI/Mobs/Mob AI) statt Radio-Buttons, Drei-Spalten-Layout (Datei+Tools / 2D View /
  3D View mit Zoom-Knöpfen). Komplette bisherige Import/Export-Funktionalität bleibt unter
  einem einklappbaren "Erweitert"-Bereich erhalten.

NEU: `include/mapeditor/app/Localization.hpp` - deutsch-zuerst, englisch bereits als
Sprachumschaltung eingebaut (Dropdown oben rechts).

**Nur syntaktisch geprüft** (`-fsyntax-only` gegen echte imgui/GLFW-Header + selbstgeschriebenen
glad-Stub, 0 Fehler/Warnungen) - kein echter Build/Lauf in dieser Sandbox möglich. Siehe
HANDOFF.md für den vollständigen Umfang und die Einschränkung.

## [0.43.1] — NiLookAtInterpolator komplett neu implementiert (+4 Dateien, 76.3%)

Bisher komplett unimplementierter Blocktyp (`H_AIRDOLL.nif`, 4 identische Kopien). Struktur
aus der autoritativen `nif.xml`: `flags(u16)` + `look_at_ref(i32, Ptr auf NiNode)` +
`look_at_name(SizedString)` + `NiQuatTransform` (`translation(12)+rotation(16)+scale(4)` = 32
Byte - "TRS Valid" entfällt seit Version 10.1.0.109, betrifft unsere 20.0.0.4 nicht) + 3
weitere Interpolator-Refs (Translation/Roll/Scale, je i32).

Byte-exakt an `H_AIRDOLL.nif` Block 144 verifiziert: Rotation ist ein exakter
Einheits-Quaternion, Translation/Scale beide `-FLT_MAX` (bekannter NIF-Sentinelwert),
`look_at_ref` zeigt exakt auf das nächste `NiNode`, alle 3 Interpolator-Refs sauber `-1` -
die berechnete Blocklänge landet exakt auf dem lesbaren Namensfeld `"Camera01.Target"` des
folgenden `NiNode`.

Massentest **2618 → 2622/3436 (+4 Dateien, 76.3%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 47.

## [0.43.0] — DURCHBRUCH: vollständige ObjectNetBase-Kettenvalidierung erkennt benannte Objekte (+128 Dateien, zweitgrößter Einzelfund der Session, 76.2%!)

An `Tree01.nif` gefunden: nach NiPixelData folgt ein BENANNTES NiNode - der bisherige 4-Byte-
Trailer-Peek erkennt nur leere Namen. Anders als der in v0.32.1 gescheiterte
`LooksLikeFreshName()`-Versuch (nur das Namensfeld geprüft, katastrophaler Rückschritt
1818→363) validiert die neue Prüfung die GESAMTE ObjectNetBase-Kette: plausibler Name (1-40
Zeichen, druckbar) UND plausibles numExtra (0-10) UND plausibler controller (-1 oder 0-300) -
alle gleichzeitig. Diese Kombination ist weit seltener zufällig erfüllt als eine bloße
Namensform.

Byte-exakt an Tree01.nif verifiziert und stichprobenartig auf konsistente Geometrie geprüft
(4 Teile, Vertex=Normalen=UV-Anzahl). Voller Regressionstest (7/7 Suiten) grün.

Massentest **2490 → 2618/3436 (+128 Dateien, 76.2%!)**. Siehe docs/MAP_FORMAT.md
Abschnitt 45.

## [0.42.2] — NiTexturingProperty: sicherer Rückfallversuch bei CountU32-Überschreitung (keine neuen Dateien, aber wichtige Absicherung)

An `adel_terrain_root_town.nif` gefunden: peek=8 wurde fälschlich als 8 echte Extra-Daten-
Refs gelesen (alle zufällig <100000, bestehen die generelle Prüfung), texture_count landete
bei ~1,6 Milliarden. Anders als der in v0.40.1 verworfene "texture_count==0"-Versuch (schwaches
Signal) wird hier auf tatsächliche CountU32-Überschreitung geprüft (starkes, praktisch nie
zufälliges Signal). Neue `ByteReader::SetOk()`-Methode ermöglicht den gezielten Rückfall.

Massentest bleibt bei 2490/3436 (kein weiterer Treffer im aktuellen Korpus), aber echte,
bewiesene Korrektur ohne Regression. Siehe docs/MAP_FORMAT.md Abschnitt 44.

## [0.42.1] — NiTriShapeData: mysteriöses Feld entfällt bei älteren Versionen komplett (+22 Dateien, 72.5%)

Beim Verfolgen eines NiSkinInstance-Fehlers (`horse2.nif`/`horse3.nif`, Version 10.2.0.0)
gefunden: anders als bei NiTriStripsData (wo nur additional_data_ref bei älteren Versionen
entfällt) fehlt bei NiTriShapeData das "mysteriöse" u16-Feld nach den UV-Daten komplett.
Byte-exakt verifiziert: ohne das Feld ergeben sich consistency_flags=0x4000 (gültig),
num_triangles=700, num_triangle_points=2100 (exakt num_triangles*3).

Massentest **2468 → 2490/3436 (+22 Dateien, 72.5%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 43.

## [0.42.0] — DURCHBRUCH: das +4-Byte-Muster generalisiert (+411 Dateien, MIT ABSTAND größter Einzelfund der Sitzung, 71.8%!)

Nach den Funden bei NiZBufferProperty/NiAlphaProperty/NiVertexColorProperty/
NiSpecularProperty (je +4 Byte vor direkt folgender NiTriStripsData/NiTriShapeData) wurde
bestätigt: auch NiFloatData als Vorgänger braucht dasselbe Muster - das Problem hängt NICHT
vom spezifischen Vorgänger-Blocktyp ab.

**Generalisierte Lösung statt weiterer Einzelaufzählung:** Eine neue Hilfsfunktion
(`LooksLikeTriDataHeader`) prüft DIREKT am Zielblock (NiTriStripsData/NiTriShapeData), ob
die aktuelle Position wie ein gültiges num_vertices/keep_flags/compress_flags/has_vertices-
Muster aussieht - unabhängig davon, was davor stand. Wenn nicht, aber 4 Byte weiter schon,
wird dort weitergelesen.

Volle Regression (7/7 Suiten) weiterhin grün, alle bisherigen Referenzdateien unverändert
korrekt. Mehrere neu erfolgreiche Dateien stichprobenartig auf konsistente Geometrie
geprüft (Vertex=Normalen=UV-Anzahl, sinnvolle Dreieckszahlen).

Massentest **2057 → 2468/3436 (+411 Dateien, 71.8%!)**. Siehe docs/MAP_FORMAT.md
Abschnitt 42.

## [0.41.2] — NiMorphData ergänzt + Diagnose-Runde (keine neuen Dateien, aber vollständige Klärung mehrerer Fälle)

`NiMorphData` aus der Referenz ergänzt (byte-exakt verifiziert an `zzz_kong.nif`). Zusätzlich
zwei weitere Fälle vollständig diagnostiziert: `LegelDungeon.nif` als weitere Instanz des
bekannten `NiPixelData`-Trailer-Sonderfalls (Abschnitt 20) bestätigt; `KDVictor.nif`s
`NiCollisionData` als korrekt verifiziert, Fehlausrichtung liegt tiefer in
`NiTriStripsData` selbst (offen für Folgesession). `NiBoneLODController` geprüft, aber wegen
Komplexität für nur 1 Datei nicht implementiert.

Massentest bleibt bei 2057/3436 (59.9%). 7/7 Test-Suiten weiterhin grün, keine Regression.
Siehe docs/MAP_FORMAT.md Abschnitt 41.

## [0.41.1] — Offener Faden einzeln durchgetestet: 3 von 5 Eigenschaftstypen sicher (+16 Dateien, 59.9%)

Die fünf in v0.41.0 zurückgenommenen Eigenschaftstypen einzeln (nacheinander mit vollem
Massentest) erneut getestet: `NiVertexColorProperty` (+14, sicher), `NiStencilProperty`
(-24, verursacht Schaden - vermutlich wegen eigener interner Freitextfeld-Mehrdeutigkeit,
sofort zurückgenommen), `NiSpecularProperty` (+2, sicher), `NiFogProperty` und
`NiDitherProperty` (je ±0, neutral aber unschädlich, übernommen).

Massentest **2041 → 2057/3436 (+16 Dateien, 59.9%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 40.

## [0.41.0] — NiAlphaProperty-Fund: größter Einzelfund dieser Session (+62 Dateien, 59.4%)

Dasselbe "+4 Byte vor NiTriStripsData/NiTriShapeData"-Muster wie bei `NiZBufferProperty`
(v0.40.2), aber diesmal bei `NiAlphaProperty` UND unabhängig von der NIF-Version bestätigt
(`Leviathan_lightA_non.nif`, Version 20.0.0.4). Generalisierte Hilfsfunktion
`SkipExtraBytesIfFollowedByTriData` ergänzt und bei `NiAlphaProperty` angewendet.

Ein Versuch, dieselbe Prüfung auch auf `NiVertexColorProperty`, `NiStencilProperty`,
`NiSpecularProperty`, `NiFogProperty`, `NiDitherProperty` anzuwenden, verursachte einen
Netto-Rückschritt (-8 Dateien) und wurde sofort komplett zurückgenommen - offen für eine
Folgesession, die jeden Typ einzeln testet.

Massentest **1979 → 2041/3436 (+62 Dateien, größter Einzelfund dieser Session, 59.4%)**.
7/7 Test-Suiten weiterhin grün, keine Regression. Siehe docs/MAP_FORMAT.md Abschnitt 39.

## [0.40.2] — Offener Faden gelöst: NiZBufferProperty vor NiTriStripsData/NiTriShapeData braucht 4 Byte (+3 Dateien, 57.6%)

Der Abschnitt-37-Faden bestätigt an einer zweiten, unabhängigen Datei (`field_sky_01.nif`,
identische Werte `flags=1/function=3`). Ein unbedingter Test (immer +4 Byte) verursachte
eine Regression (-3 Dateien) und wurde verworfen - stattdessen eng auf die Nachbarschaft zu
`NiTriStripsData`/`NiTriShapeData` begrenzt (analog zum NiPixelData-Trailer-Muster).

Massentest **1976 → 1979/3436 (+3 Dateien, 57.6%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 38.

## [0.40.1] — Dritter NiPixelData-Trailer-Fall: manchmal 0 statt 4/8 Byte (+2 Dateien)

Byte-exakt an `skeleton_monolith_blood.nif` verifiziert: nach `NiPixelData` kann manchmal
GAR KEIN Trailer nötig sein. Als zusätzliche, vor den bestehenden Fällen geprüfte Bedingung
ergänzt. Massentest **1974 → 1976/3436 (+2 Dateien, 57.5%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression.

Offener Faden dokumentiert (nicht implementiert): `NiZBufferProperty` gefolgt von
`NiTriStripsData` scheint in einem Fall 4 zusätzliche Byte zu brauchen - nur ein Beleg,
nicht ausreichend verifiziert für eine Regel. Siehe docs/MAP_FORMAT.md Abschnitt 37.

## [0.40.0] — Additional-Data-Feld entfällt bei älteren Versionen (+41 Dateien, größter Einzelfund seit ShaderTexDesc, 57.5%)

Direkte Fortsetzung von v0.39.0: `NiGeometryData`s "Additional Data"-Ref-Feld ist laut
autoritativer Referenz erst `since="20.0.0.4"` vorhanden - bei älteren Versionen
(10.1.0.0/10.2.0.0) entfällt es komplett. Bisher unbedingt gelesen, dadurch bei jeder
NiTriStripsData/NiTriShapeData in älteren Dateien eine 4-Byte-Fehlausrichtung.

Byte-exakt an `skeleton_monolith_blood.nif` verifiziert: ohne das Feld ergeben sich
plausible `consistency_flags`, eine korrekte Streifenlänge (passt exakt zur
Dreieckszahl+2) und eine klassische Dreiecksstreifen-Indexfolge - landet danach exakt auf
einem gültigen Blockanfang.

`ParseNiTriStripsData`, `ParseNiTriShapeData`, `SkipNiGeometryDataHeader` erhalten einen
`isOlderVersion`-Parameter. Massentest **1933 → 1974/3436 (+41 Dateien, 57.5%)**. 7/7
Test-Suiten weiterhin grün, keine Regression trotz Änderung an den meistgenutzten
Geometrie-Parsing-Funktionen. Siehe docs/MAP_FORMAT.md Abschnitt 36.

## [0.39.0] — DURCHBRUCH bei älteren NIF-Versionen: drei zusammenhängende Funde (+31 Dateien, 56.3%)

Ausgehend von `skeleton_monolith_blood.nif` (Version 10.2.0.0, per `strings` identifiziert)
drei zusammenhängende, version-spezifische Strukturabweichungen gefunden und behoben:

1. **`TexDesc` hat zwei zusätzliche PS2-Felder** (`PS2 L`, `PS2 K`, je short) vor Version
   10.4.0.1 - bei 20.0.0.4 bereits entfallen. `ParseNiTexturingProperty` erhält einen
   `hasPS2Fields`-Parameter.
2. **`NiPixelData`s Kopfstruktur ist für ältere Versionen 50 Byte** (nicht 72 wie bei
   20.0.0.4, auch nicht die von nif.xml beschriebenen 36/58 Byte - noch eine
   Custom-Engine-Abweichung). Durch Mipmap-Ketten-Suche empirisch gefunden und byte-exakt
   verifiziert. `SkipNiPixelData` erhält einen `isOlderVersion`-Parameter.
3. **Der 4-vs-8-Byte-Trailer-Peek nach `NiPixelData` versagt strukturell**, wenn der nächste
   Block kein Namensfeld hat (`NiTriStripsData`/`NiTriShapeData`) - als zusätzliche,
   ergänzende Bedingung (nur für ältere Versionen) implementiert. **Der größte Einzelfund
   dieser Serie: +30 Dateien.**

Massentest **1902 → 1933/3436 (+31 Dateien, 56.3%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression trotz Änderungen an mehreren zentralen Funktionen. Siehe docs/MAP_FORMAT.md
Abschnitt 35.

## [0.38.2] — Zwei wichtige Gegenproben gegen die autoritative Referenz dokumentiert (keine Funktionsänderung, weiterhin 1902/3436)

**NiPixelData:** Kopf-Struktur weicht von BEIDEN nif.xml-Versionsvarianten ab (weder die
alte "until 10.4.0.1" noch die neue "seit 10.4.0.2" Variante ergeben plausible Werte an
`BerFrz01_Ice02.nif`) - die bereits empirisch funktionierende Eigenimplementierung bewusst
NICHT verändert.

**ParseNiTriStripsHeader:** ZWEITER bestätigter Beinahe-Katastrophenversuch. Die autoritative
Referenz beschreibt `has_shader` als echt konditional (wie in `SkipNiParticleSystem` bereits
korrekt behandelt) - byte-exakt an `Leviathan_deco1.nif` verifiziert (has_shader=0,
data_ref=15 korrekt). Trotzdem verursachte die Umsetzung einen KATASTROPHALEN Rückschritt
(1902 → 1486!) - sofort zurückgenommen. Dieser custom Engine-Fork weicht auch hier von der
Vanilla-Spezifikation ab. `ParseNiTriStripsHeader` bleibt ENDGÜLTIG OFF LIMITS.

Beide Funde ausführlich in docs/MAP_FORMAT.md Abschnitt 33/34 dokumentiert, um wiederholte
Versuche zu vermeiden. Massentest weiterhin 1902/3436, 7/7 Test-Suiten grün, keine Regression
im ausgelieferten Code.

## [0.38.1] — NiGeometryData-Kernstruktur bestätigt + latenter Tangenten-Bug behoben (netto keine neuen Dateien, aber wichtige Absicherung)

Die zentrale `NiGeometryData`-Struktur gegen die autoritative `nif.xml` geprüft: Feldreihenfolge
bestätigt (keine Änderung nötig). Dabei einen latenten Bug gefunden: das "Data Flags"-Feld
wurde mit einem zu engen Plausibilitäts-Cap (`CountU16(16u)`) gelesen, der Dateien mit
gesetztem Tangenten-Bit (Bit 12) fälschlich abgelehnt hätte. Cap entfernt, Tangenten/
Binormalen-Handling ergänzt.

Massentest bleibt bei 1902/3436 (aktueller Korpus scheint keine Tangenten zu nutzen), aber
wichtige Absicherung gegen zukünftige Dateien. 7/7 Test-Suiten weiterhin grün, keine
Regression trotz Änderung an der meistgenutzten Parser-Funktion. Auch bestätigt: `key_type=0`
bei `NiPosData` ist laut autoritativer Referenz (`KeyType`-Enum: nur Werte 1-5 definiert)
genuinely ungültig - betrifft 38 Kopien derselben `EnvSet.nif`-Datei quer über viele
Zonen-Ordner. Siehe docs/MAP_FORMAT.md Abschnitt 32.

## [0.38.0] — ShaderTexDesc implementiert (+34 Dateien, größter Einzelfund seit dem nif.xml-Durchbruch, 55.4%)

`NiTexturingProperty`s `num_shader_textures` führte bisher bei jedem Nicht-Null-Wert zu
sauberem Abbruch ("nie in Testdaten beobachtet"). Jetzt implementiert: `ShaderTexDesc` =
`has_map(bool)` + `[map(TexDesc) + map_id(u32)]`. Byte-exakt an `bossroom_wall.nif`
verifiziert (3 Shader-Texturen, source_refs 11/13/15, map_id 0/1/2 - alles plausibel).

Zusätzlich `NiGeomMorpherController` aus der Referenz ergänzt.

Massentest **1868 → 1902/3436 (+34 Dateien, 55.4%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 31.

## [0.37.0] — NiPSysDragModifier, NiPortal ergänzt + NiTexturingProperty-Korrekturen (+9 Dateien, 54.4%)

`NiPSysDragModifier` und `NiPortal` aus der Referenz ergänzt (+8 Dateien). Dazu
`NiTexturingProperty` korrigiert: hatte eine eigene, veraltete Kopie der
`num_extra_data_refs`-Peek-Logik statt `ParseObjectNetBase()` zu nutzen (an
`AdlF_field_burn_ground.nif` gefunden) - dazu die fehlenden 24 Byte des Bump-Map-Texturslots
(Index 5) ergänzt (+1 Datei).

Ein zusätzlicher Rückfallversuch bei `texture_count==0` wurde getestet, aber wegen eines
Netto-Rückschritts (-1 Datei) sofort verworfen und dokumentiert (siehe docs/MAP_FORMAT.md
Abschnitt 30) - ein weiteres Beispiel für die Grenzen von Peek-Heuristiken.

Massentest **1859 → 1868/3436 (+9 Dateien, 54.4%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression.

## [0.36.1] — Wiring-Fehler behoben (NiPointLight) + fünf weitere Blocktypen (+5 Dateien, 54.1%)

**Wichtiger Fund:** `NiPointLight` aus v0.36.0 war implementiert, aber NIE in die Dispatch-
Weiche eingebunden - blieb dadurch wirkungslos. Nach Einbindung systematisch alle anderen
neuen Typen aus v0.36.0 auf korrekte Einbindung geprüft (keine weiteren Lücken).

Zusätzlich ergänzt: `NiBoolTimelineInterpolator` (= NiBoolInterpolator), `NiRoom`,
`NiPSysPlanarCollider` (neue Basis NiPSysCollider entdeckt - keine NiObjectNET-Basis!),
`NiPSysEmitterLifeSpanCtlr` (= NiPSysModifierActiveCtlr).

Massentest **1854 → 1859/3436 (+5 Dateien, 54.1%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 29.

## [0.36.0] — DURCHBRUCH: autoritative nif.xml-Referenz direkt geladen (+10 Dateien, 54.0%)

### Effizienzgewinn
Die offizielle `niftools/nifxml`-Referenzdatei (nif.xml, 8563 Zeilen) direkt von
`raw.githubusercontent.com` heruntergeladen (auf der Netzwerk-Allowlist, anders als
`github.com` selbst) - ermöglicht präzises lokales Nachschlagen statt einzelner,
unvollständiger Web-Suchen.

### GROSSER FUND: NiTexturingProperty's num_shader_textures ist UNBEDINGT vorhanden
Widerlegt die Abschnitt-7/8-Regel ("nur bei controller_ref != -1") - laut autoritativer
Referenz gilt seit Version 10.0.1.0 keine Bedingung. Bedingung entfernt.
**Massentest 1849 → 1854 (+5 Dateien).**

### NiPSysMeshEmitter: empirischer Kompromiss durch exakte Struktur ersetzt
Der 244-Byte-Kompromiss aus v0.32.0 (dokumentiertes Restrisiko) war zudem durch
zwischenzeitliche andere Fixes bereits veraltet. Jetzt exakt: num_emitter_meshes + Refs +
initial_velocity_type + emission_type + emission_axis.

### Zwölf weitere Blocktypen ergänzt
`NiPointLight`, `NiSortAdjustNode`, `NiRoomGroup`, `NiPalette` (keine NiObjectNET-Basis!),
`NiVisController`, `NiPSysColliderManager`, `NiIntegersExtraData`,
`NiMultiTargetTransformController`, `NiPSysGravityStrengthCtlr`, `NiFogProperty`,
`NiDitherProperty`, `NiSourceCubeMap` - alle direkt aus der Referenz, mehrere mit
versionsabhängigen Feldern, die bei unserer Version (20.0.0.4) entfallen.
**Massentest 1845 → 1849 (+4 Dateien).**

### Gesamtergebnis
Massentest **1844 → 1854/3436 (+10 Dateien, 54.0%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 28.

## [0.35.0] — NiPSysModifierActiveCtlr + NiFlipController ergänzt (+2 Dateien, byte-exakt verifiziert)

`NiPSysModifierActiveCtlr` (30-Byte-Basis + modifier_name, byte-exakt: target zeigt exakt auf
NiParticleSystem, Name "NiPSysDragModifier(Z-Axis):10") und `NiFlipController` (30-Byte-Basis
+ texture_slot + Textur-Ref-Liste, byte-exakt: target zeigt exakt auf NiTexturingProperty,
30 Refs bilden eine regelmäßige Folge) ergänzt.

Massentest **1842 → 1844/3436 (+2 Dateien, 53.7%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 27.

## [0.34.0] — Vier weitere einfache NiExtraData-Varianten ergänzt (+6 Dateien, 53.6%)

`NiTextKeyExtraData` (byte-exakt verifiziert: lesbare Animationskommandos "start -name
idle01 ... -loop"/"end"), `NiFloatExtraData` (byte-exakt: Name "ambient", Wert 0.0),
`NiColorExtraData` (byte-exakt: Name "paramedgecolor", Wert (1,1,1,1)) und
`NiBooleanExtraData` (strukturell analog, nicht unabhängig verifiziert) ergänzt - alle
einfache, unzweideutige Erweiterungen der bereits vorhandenen `NiExtraData`-Basis.

Massentest **1836 → 1842/3436 (+6 Dateien, 53.6%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 26.

## [0.33.2] — Weiterer Gegenbeweis zur NiTexturingProperty num_shader_textures-Regel dokumentiert (keine Funktionsänderung, weiterhin 1836/3436)

An `Eff_2.nif` widerlegt: die Regel "`num_shader_textures` nur vorhanden wenn
`controller_ref != -1`" (Abschnitt 7/8) trifft hier nicht zu (`controller_ref=-1`, Feld
trotzdem vorhanden). Byte-exakt durch volle NiNode-Plausibilitätsprüfung verifiziert
(Rotationsmatrix, Skalierung, numProps). Ein `LooksLikeFreshName()`-Peek hätte hier KEINE
Unterscheidungskraft (beide Kandidatenpositionen sehen plausibel aus) - bewusst nicht per
Peek behoben, um keine dritte Beinahe-Katastrophe zu riskieren (siehe Abschnitt 20).
Ausführlich in docs/MAP_FORMAT.md Abschnitt 25 dokumentiert.

## [0.33.1] — NiPSysMeshUpdateModifier ergänzt (netto keine neuen Dateien, aber echte Erweiterung)

`NiPSysMeshUpdateModifier` laut Referenz eine einfache, unzweideutige Struktur
(`NiPSysModifierBase` + `num_meshes(u32)` + Ref-Liste) - direkt implementiert. Betroffene
Dateien (z.B. `Eff_2.nif`) kommen jetzt deutlich weiter (Block 74 → 93), scheitern aber an
einer unabhängigen Stelle (`NiBillboardNode`, bereits implementiert, aber durch
vorausgehende Fehlausrichtung betroffen - nicht weiter untersucht). Massentest bleibt bei
1836/3436, 7/7 Test-Suiten weiterhin grün, keine Regression. Siehe docs/MAP_FORMAT.md
Abschnitt 24.

## [0.33.0] — ParseObjectNetBase weiter generalisiert: num_extra_data_refs fehlt auch bei kleinen Controller-Werten (+18 Dateien, 53.4%)

### Gefunden und behoben
Der enge Fix aus v0.30.0 (nur `0xFFFFFFFF`) erkannte nicht, dass `num_extra_data_refs` auch
fehlen kann, wenn der Controller-Wert ein KLEINER, gültiger Block-Index ist (an
`AdlFH_field_burn_ground.nif` gefunden: Controller=6, fälschlich als "6 Extra-Daten-Refs"
interpretiert, deren Werte allesamt Float-Bitmuster waren). Generalisierter, aber weiterhin
konservativer Fix: bei einem potenziellen Zähler zwischen 1 und 1000 werden jetzt die
implizierten Extra-Refs UND der folgende Controller-Wert auf Plausibilität geprüft (jeweils
-1 oder ein Wert unter 100000) - nur bei fehlgeschlagener Prüfung wird das Feld als abwesend
behandelt.

### Ergebnis
Massentest **1818 → 1836/3436 (+18 Dateien, 53.4%)**. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 23.

### Auch untersucht, ohne neue Fixes
Mehrere frische `NiTriStripsData`/`NiTriShapeData`-Fehlschläge zurückverfolgt - stellten sich
als weitere Instanzen der bereits dokumentierten, bewusst nicht behobenen `NiMaterialProperty`
14-vs-15-Float-Ambiguität heraus (Abschnitt 14), nur diesmal in beide Richtungen beobachtet
(zu wenig UND zu viele Floats je nach Datei). Bestätigt die bisherige Entscheidung, dies nicht
per Peek zu lösen.

## [0.32.2] — NiMeshPSysData: 17 Byte mehr als NiPSysData, byte-exakt (netto keine neuen Dateien, aber echte Korrektur)

`NiMeshPSysData` wurde bisher identisch zu `NiPSysData` behandelt. An `stone03.nif`
byte-exakt verifiziert: sie hat 17 zusätzliche Byte (u32+u8+u32+u32+i32, letzterer ein
gültiger Ref) vor dem nächsten Block. Massentest bleibt bei 1818/3436 (betroffene Dateien
scheitern an der bereits bekannten `NiPSysMeshEmitter`-Unsicherheit weiter hinten), aber dies
ist eine echte, bewiesene Korrektur (kein Kompromiss) - siehe docs/MAP_FORMAT.md Abschnitt
21. 7/7 Test-Suiten weiterhin grün, keine Regression.

## [0.32.1] — Gefährlicher Beinahe-Fund dokumentiert (keine Funktionsänderung, weiterhin 1818/3436)

An `BeraM_Wood3.nif` eine benannte `NiMaterialProperty` nach `NiPixelData` gefunden, die vom
bestehenden 4-vs-8-Byte-Trailer-Peek (der nur leere Objekte erkennt) falsch behandelt wird.
Ein Fix-Versuch mit dem sonst zuverlässigen `LooksLikeFreshName()` verursachte einen
KATASTROPHALEN Rückschritt (1818 → 363!) - sofort erkannt und zurückgenommen. Ursache: direkt
nach rohen Pixeldaten kommen "zufällig plausibel aussehende" kurze Namensfelder viel zu
häufig vor, damit eine reine Inhalts-Heuristik hier zuverlässig funktioniert. Ausführlich in
docs/MAP_FORMAT.md Abschnitt 20 dokumentiert, damit dieser Ansatz nicht wiederholt wird.

## [0.32.0] — NiPSysMeshEmitter: empirisch bester Kompromiss statt "nicht unterstützt" (+20 Dateien, mit dokumentiertem Restrisiko)

### Von Vorsicht zu kalkuliertem Fortschritt
Nach der Entscheidung in v0.31.1, `NiPSysMeshEmitter` NICHT zu raten, wurde die Untersuchung
auf alle 70 betroffenen Dateien ausgeweitet (statt nur 2). Ein systematischer Massentest-Scan
über Kandidaten-Skip-Längen (236-260 Byte) zeigte ein Plateau mehrerer gleichauf bester Werte
(1818/3436) statt eines einzelnen Optimums - ein klares Indiz, dass die wahre Feldlänge pro
Instanz variiert (`num_emitter_meshes`). Wert 244 gewählt.

### Bewusst dokumentiertes Restrisiko
Dies ist KEINE byte-exakt bewiesene Korrektur wie die übrigen Funde dieser Session, sondern
ein empirisch bester Kompromiss. Für Dateien mit abweichender tatsächlicher Länge sollte ein
sauberer Bounds-Check-Fehlschlag bei einem späteren Block folgen (die Werte selbst werden nie
gerendert) - ein kleines Restrisiko unbemerkt leicht verschobener Nachbargeometrie bleibt.
Ausführlich in docs/MAP_FORMAT.md Abschnitt 19 dokumentiert.

### Ergebnis
Massentest **1798 → 1818/3436 (+20 Dateien, 52.9%)**. 7/7 Test-Suiten weiterhin grün - der
Wert stieg über den gesamten getesteten Kandidatenbereich nie, sondern wurde durchgängig
gleich oder besser.

## [0.31.1] — NiPSysMeshEmitter untersucht, keine sichere Lösung gefunden (keine Funktionsänderung, weiterhin 1798/3436)

`NiPSysMeshEmitter` ist nach den Funden in v0.29.0-v0.31.0 der größte verbleibende Blocker
(70 Dateien). Empirische Untersuchung an 2 unabhängigen Dateien ergab eine Gesamtlängen-
Schätzung (~315 Byte), aber die vier laut Referenz (PyFFI) vorhandenen eigenen Felder lassen
sich damit nicht sauber (ganzzahlig) aufteilen - beide Testdateien sind zudem fast komplett
Null-gefüllt, was eine byte-exakte Verifikation verhindert. Bewusst NICHT implementiert, um
nicht für 70 Dateien unbemerkt falsch ausgerichtete Geometrie zu riskieren. Ausführlich in
docs/MAP_FORMAT.md Abschnitt 19 dokumentiert, inklusive eines konkreten Ansatzes für eine
Folgesession (Suche nach einer Datei mit nicht-trivialen Emitter-Werten als Anker).

## [0.31.0] — Dieselbe Ursache zweimal mehr gefunden: SkipNiStencilProperty + ParseNiMaterialProperty (+57 Dateien, 52.3%!)

### Gefunden und behoben
Gezielt nach weiteren Stellen gesucht, die `num_extra_data_refs` fälschlich als immer
abwesend annehmen (hartkodiertes `r.U32(); r.I32();` statt `ParseObjectNetBase()`):
- `SkipNiStencilProperty` - an `FighterDown.nif` gefunden, verschob nachfolgende Bytes um 4
- `ParseNiMaterialProperty` - dieselbe Ursache, sehr viel breiter wirksam (+42 Dateien allein)

Beide rufen jetzt `ParseObjectNetBase()` auf. Anschließend das gesamte restliche Codebase
nach ähnlichen Mustern durchsucht - keine weiteren Fundstellen mehr; alle übrigen
Property-Typen nutzten die gemeinsame Funktion bereits korrekt.

### Ergebnis
Massentest **1756 → 1798/3436 (+57 Dateien zusammen, 52.3%)**. 7/7 Test-Suiten weiterhin
grün, keine Regression. Siehe docs/MAP_FORMAT.md Abschnitt 18.

### Gesamtbilanz dieser Sitzung (v0.29.0 - v0.31.0)
1676 → 1798/3436, **+122 Dateien, +3.5 Prozentpunkte** in einer einzigen Sitzung - die
ertragreichste seit dem ursprünglichen Trailer-Fund. Alle vier Funde derselben Grundursache
(num_extra_data_refs manchmal abwesend), aber jede Fundstelle brauchte eigene Untersuchung.

## [0.30.0] — num_extra_data_refs fehlt generell in ParseObjectNetBase (+26 Dateien, über 50%!)

### Gefunden und behoben
Direkt im Anschluss an v0.29.0 zeigte sich: dasselbe Phänomen (num_extra_data_refs fehlt)
tritt nicht nur bei `NiSourceTexture` auf, sondern generell in der gemeinsamen
`ParseObjectNetBase`-Funktion (verifiziert an `FighterDown.nif`: `NiVertexColorProperty` nach
einer `NiTriStrips`). Enger gefasster Fix als bei v0.29.0: nur wenn der Wert exakt
`0xFFFFFFFF` ist, wird er als controller statt als num_extra_data_refs behandelt - legitime
kleine Extra-Daten-Zähler bleiben unangetastet.

### Ergebnis
Massentest **1715 → 1741/3436 (+26 Dateien, 50.7%)** - zum ersten Mal über 50%. 7/7
Test-Suiten weiterhin grün, keine Regression trotz des sehr breiten Einsatzbereichs dieser
zentralen, von praktisch jedem Blocktyp verwendeten Funktion. Siehe docs/MAP_FORMAT.md
Abschnitt 17.

## [0.29.0] — NiSourceTexture: num_extra_data_refs fehlt bei aufeinanderfolgenden Instanzen (+39 Dateien!)

### Gefunden und behoben
49 Dateien haben im Blockindex zwei `NiSourceTexture`-Blöcke direkt hintereinander (z.B.
Haupttextur + "Dark Map" in derselben `NiTexturingProperty`). Bei der zweiten Instanz fehlt
das `num_extra_data_refs`-Feld komplett - byte-exakt durch Rückwärtsrekonstruktion von einer
eindeutig lesbaren Datei-Endung ("fence_dark.dds") bewiesen. Risikoarmer Peek-Fix: das Feld
wird nur konsumiert, wenn sein Wert 0 ist (was in JEDER bisher erfolgreich geparsten Datei
zutraf) - andernfalls übernimmt die bestehende Mystery-Feld/controller-Logik korrekt.

### Ergebnis
Massentest **1676 → 1715/3436** (+39 Dateien, +1.1 Prozentpunkte) - der bisher größte Fund
seit dem Trailer-Fix in einer früheren Session. 39 von 49 betroffenen Dateien laden jetzt
vollständig; die restlichen 10 scheitern an unabhängigen, bereits bekannten offenen
Sonderfällen weiter hinten in der jeweiligen Datei. 7/7 Test-Suiten weiterhin grün, keine
Regression. Siehe docs/MAP_FORMAT.md Abschnitt 16.

## [0.28.0] — Header-Unterstützung für ältere NIF-Versionen (10.1.0.0/10.2.0.0)

### Gefunden und behoben
177 Dateien (170x Version 10.2.0.0, 7x Version 10.1.0.0) scheiterten bisher am Header selbst.
Ursache: bei diesen älteren Versionen fehlt das Endian-Byte zwischen `version` und
`user_version`, das bei 20.0.0.4 vorhanden ist. Byte-exakt an 3 Dateien verifiziert
(`num_blocks` traf jeweils exakt mit dem separat abgelesenen Rohbyte-Wert überein).

### Bewusst nicht weiterverfolgt
Nach dem Header-Fix kommen diese Dateien deutlich weiter, scheitern aber weiterhin - bereits
`NiSourceTexture` ist für diese Version strukturell anders aufgebaut als bei 20.0.0.4 (unsere
modernen Korrekturen für Mystery-Feld und `use_external` treffen hier nicht zu). Eine
vollständige Unterstützung dieser älteren NIF-Ära wäre ein eigenständiges, größeres
Reverse-Engineering-Projekt - siehe docs/MAP_FORMAT.md Abschnitt 15 für den Stand und eine
Liste offener Fragen für eine Folgesession.

### Ergebnis
Massentest unverändert bei 1676/3436 (der Fix schaltet noch keine neuen Dateien frei, legt
aber die Grundlage für eine Folgesession). 7/7 Test-Suiten weiterhin grün, keine Regression -
rückwärtskompatibel, da nur Dateien betroffen sind, die vorher ohnehin komplett fehlschlugen.

## [0.27.1] — Zwei Untersuchungen ohne Erfolg dokumentiert (keine Funktionsänderung, weiterhin 1676/3436)

Zwei weitere Verdachtsstellen für Sonderfälle wurden untersucht und beide verworfen, da sie
im Massentest zu Rückschritten führten (jeweils sofort zurückgenommen, Code unverändert
gegenüber v0.27.0):
- `ParseNiTriStripsHeader`s Freitextfeld bei `CynDN_Tree00.nif` - eine Korpus-weite Auszählung
  (14560 Instanzen) zeigte, dass das fragliche Byte in 99.5% der Fälle 0 ist und nicht
  zuverlässig mit der An-/Abwesenheit des Freitextfelds korreliert - vermutlich ein echter
  Einzelfall, keine systematische Regel.
- `NiMaterialProperty`s 14-vs-15-Float-Frage bei `S_Tower02_ScanLine.nif` - ein
  Peek-basierter Versuch (Rückschritt 1676 → 1508) zeigte, dass die `LooksLikeFreshName()`-
  Heuristik bei aus Fließkommazahlen bestehenden Feldern unzuverlässig ist.

Beide Fälle ausführlich in docs/MAP_FORMAT.md Abschnitt 14 dokumentiert, damit sie nicht erneut
versucht werden. 7/7 Test-Suiten weiterhin grün, Massentest unverändert bei 1676/3436.

## [0.27.0] — NiSkinInstance/NiSkinData/NiSkinPartition implementiert + Trailer-Peek-Fix (1676/3436 ladbar)

### Hinzugefügt
Geskinnte Meshes: `NiSkinInstance` (data_ref + skin_partition + skeleton_root +
Knochen-Referenzen), `NiSkinData` (Bindungspose je Knochen: NiTransform + Bounding-Sphere +
optionale Vertex-Gewichte), `NiSkinPartition` (GPU-Partitionierung für Hardware-Skinning -
Strips/Dreiecke/Knochen-Indizes je Partition). Aus derselben Referenzimplementierung
übernommen. Der Editor stellt Meshes weiterhin nur in Bindungspose dar (keine Animation) -
die Blöcke werden korrekt übersprungen, nicht ausgewertet.

### Gefunden und behoben (Trailer-Logik aus v0.25.0)
- Eine `NiTriShapeData`/`NiTriStripsData` gefolgt von `NiSkinInstance` braucht ebenfalls
  keinen 8-Byte-Trailer - aber `NiSkinInstance` beginnt nicht mit einem Namensfeld, weshalb der
  bestehende `LooksLikeFreshName()`-Peek diesen Fall nicht selbst erkennen konnte. Behoben durch
  expliziten Ausschluss (analog zu NiTriStrips/NiTriShape).
- Dabei einen zweiten, subtileren Bug im bestehenden Peek-Override gefunden: er wirkte
  fälschlich auch im "expliziter Ausschluss"-Zweig und konnte dort eine korrekte,
  typ-basierte Entscheidung des Aufrufers wieder rückgängig machen. Der Override greift jetzt
  nur noch im Standardfall - eine Vereinfachung, die die Trailer-Logik insgesamt robuster
  macht.
- Byte-exakt verifiziert an `Tunnel02_Wood3.nif`: Knochenzahl der NiSkinInstance sprang von
  absurd 126 auf plausible 14, data_ref/skin_partition treffen exakt auf die folgenden Blöcke.

### Ergebnis
Massentest 1664 → 1676/3436 (48.4% → 48.8%). 7/7 Test-Suiten weiterhin grün, weiterhin 0
unplausible UVs, 303 real gefundene Texturen (unverändert).

## [0.26.0] — NiAmbientLight/NiSpecularProperty/NiPathInterpolator + ZWEITER GROSSER FUND: NiSourceTexture bei externen Texturen (1664/3436 ladbar, +199 Dateien)

### Hinzugefügt
- `NiAmbientLight` (reine `NiLight`-Basis, wie `NiDirectionalLight`)
- `NiSpecularProperty` als unterstützte Property - laut Referenz nur `NiObjectNET` + ein
  einzelnes `flags`(u16)-Feld, viel einfacher als der vorherige (nie verifizierte)
  8-Felder-Verdacht bei `NiStencilProperty`
- `NiPathInterpolator` (Pfad-/Wegpunkt-Animationen)

### GROSSER FUND: NiSourceTexture bei `use_external=1`
Bei rein externen Texturen (kein eingebettetes `NiPixelData` in der Datei) folgen nach der
Dateinamens-Referenz 18 weitere Byte (`pixel_layout`, `mipmap_format`, `alpha_format`,
`is_static`, `direct_render`, ein abschließendes Feld) - bei `use_external=0` (eingebettetes
`NiPixelData` vorhanden) fehlen diese komplett. Byte-exakt verifiziert an `RockD8.nif`.
Zusammen mit dem Trailer-Fund aus v0.25.0 hat das den Massentest an einem Nachmittag von 31.5%
auf 48.4% mehr als verdoppelt.

### Ergebnis
Massentest 1500 → 1664/3436 (43.7% → 48.4%). 7/7 Test-Suiten weiterhin grün, 0 unplausible
UVs, real gefundene Texturen 122 → 303.

### Versucht und verworfen
Der `has_shader`-Fix aus der Partikelsystem-Arbeit (v0.24.0) wurde versuchsweise auch auf
`ParseNiTriStripsHeader` (gemeinsame Basis für NiTriStrips/NiTriShape) angewendet - verursachte
einen katastrophalen Rückschritt (1664 → 4 im Massentest), sofort zurückgenommen. Siehe
docs/MAP_FORMAT.md Abschnitt 12 für Details - diese Funktion bewusst nicht weiter angefasst.

## [0.25.0] — GROSSER FUND: NiTriStripsData/NiTriShapeData-Trailer-Regel war zu eng gefasst (1465/3436 ladbar, +382 Dateien)

### Gefunden und behoben
Beim Ergänzen von `NiTextureEffect`/`NiDirectionalLight` fiel auf: die bisherige Regel
("8-Byte-Trailer fehlt NUR, wenn direkt ein weiteres NiTriStrips/NiTriShape folgt") war
unvollständig - der Trailer fehlt auch vor JEDEM ANDEREN Geschwister-Block (z.B. einem Licht,
einem Effekt, einem weiteren NiNode-Ast), nicht nur vor einem zweiten Mesh-Teil. Byte-exakt
verifiziert an `ItemShop02.nif` (NiTriStripsData gefolgt von NiDirectionalLight `"__MAX_
Default_Light"`).

### Neue Allzweck-Heuristik
`LooksLikeFreshName()`: prüft, ob eine Position plausibel der Anfang eines `SizedString`-
Namensfelds ist (Länge 0, oder 1-64 mit ausschließlich druckbarem ASCII danach - praktisch
jeder Blocktyp beginnt mit so einem Feld). Die bisherige "next==TriStrips/TriShape"-Regel
bleibt Standardfall, wird aber per Peek verworfen, wenn sie keinen plausiblen Namensanfang
ergibt, während die andere Variante sehr wohl einen liefert. Gleiches Muster wie der
`NiPixelData`-Trailer-Fix aus v0.24.0.

### Hinzugefügt
`NiTextureEffect` (70 Dateien) und `NiDirectionalLight`/`NiLight`-Basis (38 Dateien) - der
eigentliche Auslöser für diesen Fund.

### Ergebnis
**Massentest 1083 → 1465/3436 (31.5% → 42.6%) - der mit Abstand größte Einzelfund dieser
Session.** 7/7 Test-Suiten weiterhin grün, Texturierungs-Pipeline weiterhin bei 0 unplausiblen
UVs (122 real gefundene Texturen, vorher 94).

## [0.24.0] — Partikelsystem-Familie implementiert (NiParticleSystem, 1083/3436 ladbar)

### Hinzugefügt
Vollständige Partikelsystem-Blockkette: `NiParticleSystem`/`NiMeshParticleSystem`,
`NiPSysData`/`NiParticlesData` (inkl. gemeinsamem `NiGeometryData`-Kopf für Vertex-/Normalen-/
Farb-/UV-Daten), `NiPSysEmitterCtlr`/`NiPSysUpdateCtlr`, `NiBoolInterpolator`/`NiBoolData`,
`NiColorData`, sowie die Modifier `NiPSysAgeDeathModifier`, `NiPSysBoxEmitter`,
`NiPSysSpawnModifier`, `NiPSysGrowFadeModifier`, `NiPSysColorModifier`,
`NiPSysRotationModifier`, `NiPSysGravityModifier`, `NiPSysPositionModifier`,
`NiPSysBoundUpdateModifier`. Aus derselben Referenzimplementierung wie v0.20-0.23 übernommen
und einzeln byte-exakt verifiziert.

### Gefunden und behoben
- `NiParticleSystem`s `has_shader`-Byte ist echt konditional (anders als bei NiTriStrips/
  NiTriShape, wo zufällig immer Inhalt folgte) - eigene, korrekte Kopf-Funktion für Partikel.
- `KeyType`-Wert 5 (CONST) fehlte in beiden KeyGroup-Skip-Funktionen (nur 1/2/3 behandelt) -
  ergänzt, gleiche Schlüsselgröße wie LINEAR.
- `currentMeshHasTexturing` wurde nie für `NiParticleSystem` aus dessen eigener Properties-
  Liste aktualisiert (nur für NiTriStrips/NiTriShape) - genereller Bugfix, unabhängig vom
  Partikel-Thema relevant für jedes texturierte Partikelsystem.
- Nebenfund: `NiPixelData`s Abschluss-Trailer ist nicht immer 8 Byte (mind. 1 Datei mit
  unkomprimiertem 8-Bit-Format brauchte nur 4) - per Peek robust gelöst, rückwärtskompatibel.

### Offen
Bei mindestens einer Datei bleibt nach allen Fixen noch ein weiterer, nicht isolierter Versatz
vor `NiPSysData` bestehen - vermutlich ein vierter, noch unentdeckter Sonderfall (evtl.
`NiVertexColorProperty`-Länge im Partikel-Kontext). Guter Kandidat für eine Folgesession.

### Ergebnis
Massentest 1080 → 1083/3436 (31.4% → 31.5%). 7/7 Test-Suiten weiterhin grün, keine Regression,
Texturierungs-Pipeline weiterhin bei 0 unplausiblen UVs.

## [0.23.0] — NiLODNode/NiRangeLODData implementiert (1080/3436 ladbar)

### Hinzugefügt
- **`NiLODNode`**: `NiSwitchNode : NiNode` + `switch_flags`(u16) + `index`(u32) +
  `lod_level_data_ref`(i32). Rendering-seitig wie normaler `NiNode` behandelt (keine echte
  Distanz-basierte LOD-Umschaltung nötig). Byte-exakt verifiziert an `tree05.nif`
  (Name="LODGroup01", 3 Kinder, `lod_level_data_ref` trifft exakt).
- **`NiRangeLODData`**: weicht von der öffentlichen Referenzstruktur ab (kein Vector3-Center in
  diesem Fork) - stattdessen führendes u32(=0) + `num_lod_levels` + Level-Paare (near/far) +
  8-Byte-Trailer. Byte-exakt an 2 Dateien verifiziert (identische 3-Stufen-LOD-Konfiguration,
  vermutlich ein Baum-Preset) - beide Male trifft die berechnete Länge exakt auf das jeweilige
  Dateiende.

### Ergebnis
Massentest 1062 → 1080/3436 (30.9% → 31.4%). `NiRangeLODData` verschwindet komplett aus der
Fehlerliste. `NiLODNode` selbst bleibt bei ~9 Dateien noch blockiert (vermutlich weitere,
unabhängige Strukturvarianten - nicht untersucht). 7/7 Test-Suiten weiterhin grün.

## [0.22.0] — Folgefehler aus v0.21.0 gelöst: NiSourceTexture hatte dasselbe konditionale Muster (1062/3436 ladbar)

### Gelöst
Der in v0.21.0 gefundene Folgefehler (mehrfach texturierte Wasser-/Lava-/Effekt-Objekte
scheiterten an einer zweiten `NiSourceTexture`) ist behoben. Ursache: `NiSourceTexture`s
`NiObjectNET`-Basis hat - genau wie `NiTexturingProperty` in v0.21.0 - ein zusätzliches,
konditionales u32-Feld (empirisch immer 0) zwischen der Extra-Daten-Liste und dem
`controller`-Feld. Byte-exakt an 2 Dateien verifiziert (`santuary.nif`: Feld vorhanden, 17
Byte insgesamt; `SD_Vale01_machine02.nif`: Feld fehlt, 13 Byte insgesamt). Per Peek robust
unterscheidbar, rückwärtskompatibel zur bisherigen Mehrheit der Dateien.

Bewusst NUR dieses eine Feld korrigiert, nicht die komplette (umfangreichere)
`NiSourceTexture`-Struktur aus derselben Referenzimplementierung übernommen, da diese der
eigenen, seit Monaten extensiv verifizierten Blockreihenfolge (NiPixelData folgt direkt ohne
Zwischenraum) widerspricht - Lehre: jede Korrektur aus einer Referenz einzeln byte-exakt
verifizieren, nicht die ganze Struktur ungeprüft übernehmen.

### Ergebnis
Massentest 1047 → 1062/3436 (30.5% → 30.9%). Texturierungs-Verifikation bestätigt weiterhin 0
unplausible UVs bei jetzt 94 real gefundenen und dekodierten Texturen (vorher 91). 7/7
Test-Suiten weiterhin grün, keine Regression.

## [0.21.0] — NiTextureTransformController-Rätsel GELÖST (mit derselben Referenz wie v0.20.0)

### Gelöst
Das in der Vorsession als "strukturell inkonsistent" zurückgestellte `NiTextureTransformController`
ist tatsächlich **immer fest 39 Byte** - keine Variation zwischen Instanzen. Die wahre Ursache:
`NiTexturingProperty` hat nach den 7 Textur-Slots ein zusätzliches, **konditionales** u32-Feld
(vermutlich `num_shader_textures`), vorhanden NUR wenn die Property einen Controller referenziert
(`controller_ref != -1`). Byte-exakt an 4 echten Dateien verifiziert (2× mit, 2× ohne Controller
- jeweils exakt passend). Gefunden mit derselben unabhängigen Referenzimplementierung, die auch
das UV-Rätsel aus v0.20.0 löste.

### Ergebnis
`NiTextureTransformController` verschwindet komplett aus der Fehlerliste des Massentests
(vorher 134 Dateien direkt blockiert). Gesamt-Massentest bleibt bei 1047/3436, da die
betroffenen Dateien (mehrfach texturierte Wasser-/Lava-/Effekt-Objekte) jetzt an einem neuen,
strukturell ähnlichen Folgefehler bei einer zweiten `NiSourceTexture` scheitern (4-Byte-Versatz,
vermutlich ein weiteres konditionales Feld - nicht weiter untersucht, siehe docs/MAP_FORMAT.md
Abschnitt 7). 7/7 Test-Suiten weiterhin grün, keine Regression.

## [0.20.0] — UV-Rätsel aus v0.19.0 GELÖST: NIF-Objekt-Texturierung ist jetzt echt nutzbar

### Gelöst
Der in v0.19.0 gefundene UV-Bug (siehe dort) ist behoben. Auslöser war ein Hinweis des
Nutzers auf NifTools-Produkte (NifSkope u.a.) als unabhängige, funktionierende Referenz für
dieses Dateiformat. Eine öffentliche Referenzimplementierung (Rust-NIF-Parser, Ziel-Version
20.0.0.4) zeigte: das bisher direkt VOR den UV-Daten gelesene "uv_flags"-u16-Feld existiert an
dieser Stelle gar nicht - `vertex_colors` wird direkt von den UV-Sets gefolgt. Das
mysteriöse 2-Byte-Feld gehört stattdessen NACH die UV-Daten, direkt vor `consistency_flags`.
Zusätzlich: `num_uv_sets` muss mit `& 0x3F` maskiert werden (die oberen Bits gehören zu einem
separaten `tspace_flag`, bisher ohne Auswirkung, da in allen Testdateien 0).

Byte-exakt verifiziert an `santuary.nif`: alle 86 UV-Paare sind jetzt plausible, normalisierte
Texturkoordinaten (z.B. `(0.213, 0.015)`), UND alle nachfolgenden Felder treffen weiterhin exakt
bis zum Dateiende. Zusätzlich an `BerFrz_Thorn.dds`-Dateien bestätigt (saubere
0.0/0.5-Grid-UVs, passend zu einer Textur-Atlas-Aufteilung).

### Ergebnis
**Die Objekt-Texturierung aus v0.15/v0.16 ist erstmals echt nutzbar** - die in v0.19.0
eingebaute `SanitizeUvs()`-Sicherung greift jetzt bei keiner der 91 real gefundenen und
dekodierten Texturen mehr ein (vorher: 89-91 von 91). Massentest unverändert bei 1047/3436
(betrifft nur UV-Inhalte, nicht den Lade-Erfolg). 7/7 Test-Suiten weiterhin grün.

### Methodische Lehre
Byte-Längen-Verifikation über das Dateiende hinweg kann zwei kompensierende Fehler (ein
fehlendes Feld hier, ein überzähliges dort) nicht erkennen, wenn beide dieselbe Gesamtlänge
ergeben - siehe docs/MAP_FORMAT.md Abschnitt 6. Erst eine zusätzliche Inhalts-
Plausibilitätsprüfung deckte den echten Fehler auf.

## [0.19.0] — Wichtiger Fund: NIF-UV-Koordinaten sind unbrauchbar (Absicherung eingebaut, Massentest unverändert bei 1047/3436)

### Untersucht
End-to-End-Verifikation der Objekt-Texturierungs-Pipeline gegen echte Daten: alle 415 DDS- und
741 BMP-Texturdateien aus den bereitgestellten resmap-Archiven extrahiert und indiziert, dann
für jedes erfolgreich geladene `.nif`-Mesh die referenzierte Textur gesucht, mit `DdsImage`
dekodiert und die UV-Koordinaten auf Plausibilität geprüft (91 Treffer, alle DDS-Dekodierungen
fehlerfrei).

### Gefunden
**Die aus `.nif`-Meshes extrahierten UV-Koordinaten sind praktisch überall unbrauchbar** -
auch bei `santuary.nif`, dem am gründlichsten verifizierten Referenzobjekt des Projekts. Die
Byte-LÄNGE des UV-Abschnitts ist zweifelsfrei korrekt (alle Felder danach treffen exakt bis
zum Dateiende), und alle Daten VOR den UVs sind einwandfrei (86/86 Normalen exakte
Einheitsvektoren, plausible Bounding-Sphere) - die UV-WERTE selbst sind trotzdem astronomisch
große oder winzige Zahlen. Mehrere alternative Dekodierungen getestet und verworfen
(Halb-Präzision-Floats, Vector3 statt Vector2, Struct-of-Arrays, vertex-majore Anordnung).
Ursache bleibt ungeklärt - siehe docs/MAP_FORMAT.md, Abschnitt 5, für die vollständige
Herleitung und Liste ausgeschlossener Erklärungen.

### Korrigiert (Absicherung)
Neue `SanitizeUvs()`-Prüfung: verwirft ein extrahiertes UV-Set komplett, wenn auch nur ein
Wert nicht endlich oder implausibel groß ist (`|u|,|v| > 1000`). Der Renderer fällt dann
automatisch auf die Materialfarbe zurück (bereits vorhandener Mechanismus). **Deaktiviert
damit effektiv die in v0.15/v0.16 gebaute Objekt-Texturierung für praktisch alle echten
Objekte**, bis die eigentliche Ursache gefunden ist - bewusste Qualitätsentscheidung: lieber
korrekt eingefärbt als sicher falsch texturiert.

### Ergebnis
Massentest unverändert bei 1047/3436 (30.5%) - diese Änderung betrifft nur die UV-Werte
innerhalb bereits erfolgreich geladener Meshes, nicht den Lade-Erfolg selbst. 7/7 Test-Suiten
weiterhin grün.

## [0.18.0] — NiMaterialColorController + NiPoint3Interpolator + NiPosData implementiert (1047/3436 ladbar, unverändert - siehe Begründung)

### Hinzugefügt (.nif)
- **`NiMaterialColorController`**: dieselbe 30-Byte-`NiSingleInterpController`-Basis wie
  `NiAlphaController`/`NiTransformController`, plus `target_color`(u16)-Feld
- **`NiPoint3Interpolator`**: aktueller Wert(Vector3) + `data_ref`
- **`NiPosData`**: einzelne `KeyGroup<Vector3>` (analog zu `NiFloatData`)
- Byte-exakt verifiziert UND inhaltlich bestätigt an `Eff_2.nif`: eine pulsierende
  Effekt-Farbanimation über 5 Sekunden, deren Werte exakt zwischen Interpolator und
  referenzierter Keyframe-Daten übereinstimmen

### Ergebnis
Massentest unverändert bei 1047/3436 (30.5%) - `NiMaterialColorController` und
`NiPoint3Interpolator` verschwinden komplett aus der Fehlerliste (korrekt implementiert), aber
jede betroffene Datei hat mindestens einen weiteren, noch nicht unterstützten Blocker (meist
`NiTextureTransformController`). Trägt automatisch zu weiteren erfolgreichen Dateien bei,
sobald einer der verbleibenden Blocker gelöst wird. Keine Regression (7/7 Test-Suiten grün).

### Neuer offener Sonderfall
28 Dateien mit `NiPosData` bei `key_type=0` (laut offiziellem Enum ungültiger Wert) - weder
LINEAR- noch QUADRATIC- noch TBC-Interpretation ergab ein plausibles Muster. Schlägt bereits
korrekt sauber fehl (kein Rateversuch, keine Auswirkung auf andere `KeyGroup`-Nutzungen). Siehe
docs/MAP_FORMAT.md.

## [0.17.0] — NiTriShape/NiTriShapeData implementiert + ein weiterer latenter Bug in "verifiziertem" Code gefunden (1047/3436 ladbar)

### Hinzugefügt (.nif)
- **`NiTriShape`**: teilt sich den Kopf byte-exakt mit `NiTriStrips` (`NiTriBasedGeom`-Basis) -
  bewusst als separate Funktion dupliziert statt geteilt, um den bereits verifizierten
  `NiTriStrips`-Pfad nicht anzufassen
- **`NiTriShapeData`**: teilt sich Vertex-/Normalen-/Farben-/UV-Kopf mit `NiTriStripsData`,
  divergiert danach zu einer flachen Dreiecksliste statt Streifen (`num_triangle_points` +
  `has_triangles` + Indizes + Match-Groups). Verifiziert an `BerFrz01_IceSmog.nif`: 3
  aufeinanderfolgende Mesh-Teile, alle mit korrekten Dreiecks-Indizes (max. Index exakt
  `vertexCount-1`, keine Out-of-Bounds-Zugriffe)

### Korrigiert (.nif)
- **Latenter Bug in bereits seit v0.13 "verifiziertem" Code gefunden**: `Num Vertices` (in
  `NiTriStripsData` UND `NiTriShapeData`, geteilte Basisklasse) ist ein **uint16**, gefolgt von
  **Keep Flags(u8) + Compress Flags(u8)** - nicht ein einzelnes uint32 wie bisher angenommen.
  Der alte Read funktionierte nur, weil diese beiden Flag-Bytes in allen bisher getesteten
  Dateien zufällig 0 waren. Gefunden an `BerFrz01_IceSmog.nif` (Keep-Flags≠0): mit dem alten
  Read ergab sich eine absurde Vertex-Anzahl (3,3 Millionen), mit der Korrektur 72 plausible,
  radialsymmetrische Vertex-Koordinaten. Reiner Gewinn im Massentest (+39 Dateien), keine
  Regression bei vorher erfolgreichen Dateien
- **`NiTexturingProperty`**: manche Instanzen (bisher nur bei `NiTriShape`-referenzierten
  Texturen beobachtet) nutzen eine kurze 8-Byte-Basis statt der vollen 12-Byte-`ObjectNetBase`
  - per Peek-Erkennung robust behandelt, ohne den Normalfall zu beeinträchtigen

### Ergebnis
**Massentest über alle 3436 echten Dateien: 1047 laden (30.5%)**, vorher 1001 (29.1%).

### Zurückgerollter Fehlversuch (dokumentiert, kein Codeeinfluss)
Kurzzeitige Hypothese, der bekannte 8-Byte-`NiPixelData`-Trailer sei manchmal nur 4 Byte lang,
wurde getestet und verursachte einen massiven Einbruch (1008→85 Dateien) - sofort
zurückgerollt. Der 8-Byte-Trailer bleibt unverändert korrekt für die weit überwiegende
Mehrheit. Siehe docs/MAP_FORMAT.md für Details.

### Bekannter offener Sonderfall
`NiTriShapeData`/`NiTriStripsData` mit `num_vertices=0` (vermutlich geskinnte Meshes) scheitern
weiterhin (~26 Dateien) - vermutlich werden die optionalen Normalen-/Farben-/UV-Datenblöcke bei
0 Vertices komplett ausgelassen statt als leere Flags serialisiert. Nicht abschließend
verifiziert, siehe docs/MAP_FORMAT.md.

## [0.16.0] — Weitere .nif-Blocktypen: Kollision, Extra-Daten, Transform-/Alpha-Animation (1001/3436 ladbar) + ein latenter Texturing-Bug behoben

### Hinzugefügt (.nif)
- **`NiCollisionData`** (Box/Sphere/Capsule-Bounding-Volumes) - größter Einzel-Hebel, löste
  allein einen Großteil der zuvor blockierten Dateien
- **`NiStringExtraData`** + **`NiIntegerExtraData`** (eigene, von `NiObjectNET` abweichende
  Basis: nur ein Namensfeld, kein Extra-Daten-Zähler, kein Controller)
- **`NiBillboardNode`** (wie `NiNode` + `billboard_mode`-Feld)
- **`NiTransformController` → `NiTransformInterpolator` → `NiTransformData`**: komplette
  Transform-Keyframe-Kette inkl. `XYZ_ROTATION_KEY`-Zweig mit gemischten Interpolationstypen -
  inhaltlich bestätigt an einer schaukelnden Blumen-Animation (`AdlF_Flower.nif`)
- **`NiAlphaController` + `NiFloatInterpolator` + `NiFloatData`**: Alpha-Keyframe-Animation -
  inhaltlich bestätigt an einem flackernden Brand-Effekt (`AdlFH_field_burn_ground.nif`)
- Alle neuen Blocktypen per Landmarken-Technik byte-exakt gegen mehrere echte, strukturell
  unterschiedliche Dateien verifiziert (siehe docs/MAP_FORMAT.md für Details und
  Referenzdateien)

### Korrigiert (.nif)
- **Latenter Bug in bereits "verifiziertem" Code gefunden**: `NiTexturingProperty`s
  pro-Slot-Textur-Transform ist 32 Byte (8 Felder, inkl. eines zuvor übersehenen
  `transform_type`-u32-Feldes zwischen Rotation und Center), nicht 28 Byte (7 Felder) wie
  bisher angenommen. Betraf potenziell auch die in v0.15.0 gebaute, noch unverifizierte
  Objekt-Texturierung bei UV-transformierten Texturen. Gefunden an
  `SD_Vale01_machine02.nif` (4 transformierte Slots in einer Property) - siehe
  docs/MAP_FORMAT.md
- **`NiIntegerExtraData`**: seltenes führendes -1-Feld (~1% der Instanzen, bisher nur bei einem
  benannten Multi-Textur-Blend-Shader beobachtet) per sicherem `PeekU32`-Check erkannt und
  übersprungen, ohne die übrigen ~99% der Instanzen zu beeinträchtigen

### Ergebnis
**Massentest über alle 3436 echten Dateien: 1001 laden (29.1%)**, vorher 579 (16.9%) -
0 Abstürze, alle nicht unterstützten Fälle scheitern weiterhin sauber.

### Bewusst NICHT umgesetzt: `NiTextureTransformController`
Struktur erwies sich als inkonsistent zwischen Instanzen (39 vs. 43 Byte für augenscheinlich
identisch aufgebaute, aufeinanderfolgende Vorkommen im selben Objekt), ohne auffindbares
Unterscheidungsmerkmal. Um das Risiko stiller Datenkorruption zu vermeiden, bleibt dieser Typ
bewusst nicht unterstützt (110 von 3436 Dateien blockiert) - vollständige Analyse und nächste
Schritte in docs/MAP_FORMAT.md.

## [0.15.0] — Objekte werden jetzt texturiert + zwei weitere .nif-Fixes (579/3436 ladbar)

### Hinzugefügt
- **`NifMeshRenderer` lädt jetzt echte Diffuse-Texturen für Objekte** (dieselbe `DdsImage`-
  Infrastruktur wie die Terrain-Texturen) - sofern ein Mesh-Teil sowohl eine
  Textur-Dateireferenz als auch eigene UV-Koordinaten mitbringt. Vertex-Puffer um UV-Attribut
  erweitert, Textur-Cache über Modelle/Karten hinweg. Ohne Textur/UVs bleibt der Fallback
  (Materialfarbe) unverändert. **Nicht verifiziert:** ob NIF-UVs dieselbe V-Achsen-Behandlung
  wie die berechneten Terrain-UVs brauchen (siehe docs/MAP_FORMAT.md)

### Korrigiert (.nif)
- **`NiStencilProperty` byte-genau vermessen**: kurze Basis (8 Byte) + 7 uint32-Felder (nicht
  8, wie zunächst angenommen) + 1 Einzelbyte + eingebettetes Namensfeld
- **Zweiter konditionaler Trailer gefunden**: der 8-Byte-Trailer am Ende von `NiTriStripsData`
  fehlt (wie schon bei `NiPixelData` bekannt), wenn direkt ein weiteres `NiTriStrips` folgt -
  betrifft Mehrfach-Mesh-Objekte (z.B. `Rou_M_Tube.nif`, `rouval_Tower.nif`)
- **Massentest über alle 3436 echten Dateien: 579 ladbar (16.9%)**, vorher 257 (7.5%) - mehr
  als verdoppelt durch diese zwei Fixes
- `tests/test_nif_model.cpp` aktualisiert: die zweite Testdatei muss nicht mehr zwingend
  fehlschlagen (vorher hart erwartet, jetzt da die Parser-Abdeckung wächst als möglicher
  Erfolg mit Geometrie-Validierung behandelt)

## [0.14.3] — Diffuse-Textur war oben/unten vertauscht (gezielter V-Flip, Blend unverändert)

### Korrigiert
- **Diffuse-Textur war relativ zur Blend-/Heightmap-Struktur oben/unten vertauscht.** Vom
  Nutzer anhand eines Screenshots klar identifiziert: oberer Bildbereich = korrekte Struktur
  (Blend/Heightmap), unterer Bildbereich = Diffuse-Textur, beide systematisch gegeneinander
  gespiegelt - durchgängig bei allen Texturen, nicht nur einem Einzelfall
- Die V-Achse wird jetzt NUR für die Diffuse-Textur-Zuordnung gespiegelt
  (`vec2(mapUv.x, 1.0 - mapUv.y)`), der Blend-Gewichts-Lookup bleibt unverändert bei `mapUv`
  (dessen Ausrichtung war bereits vor dieser Session an Heightmap/Objekt-Positionen validiert).
  Die in v0.14.2 korrigierte Kachel-Skalierung (UVScaleDiffuse als Wiederholungsanzahl über die
  Kartenfläche) bleibt unverändert bestehen - beide Fixes zusammen ergeben jetzt korrekt
  orientierte, korrekt skalierte Diffuse-Texturen

## [0.14.2] — UVScaleDiffuse-Formel korrigiert (Nutzer-Screenshots zeigten vervielfachtes Emblem)

### Korrigiert
- **Diffuse-Textur-Kachelung war fest auf 500 Welteinheiten verdrahtet**, unabhängig von der
  Kartengröße - bei größeren Karten (z.B. Rou, 12800 Einheiten Spannweite) wiederholte sich
  jede Bodentextur ca. 100x statt der durch echte `UVScaleDiffuse`-Werte (4-5) vermutlich
  beabsichtigten 4-5x. Nutzer-Screenshots zeigten dadurch ein eigentlich einmaliges
  kreisrundes Boden-Emblem mehrfach wiederholt
- **Korrigierte Formel**: `UVScaleDiffuse` wird jetzt als Wiederholungsanzahl ÜBER DIE GESAMTE
  KARTENFLÄCHE interpretiert (`uv = weltposition/kartenspanne * UVScaleDiffuse`) statt als
  Kehrwert einer festen 500-Einheiten-Periode - besser durch echte `.ini`-Werte gestützt
- Die zwischenzeitlich probeweise eingebaute DirectX/OpenGL-V-Flip-Spekulation für die
  Diffuse-Textur wurde zurückgenommen (unbestätigt, durch diesen fundierteren Fix ersetzt)

### Hinweis zur Sandbox
Die Entwicklungsumgebung wurde zwischen Sessions zurückgesetzt - der Code-Stand wurde aus dem
zuletzt ausgelieferten Paket (v0.14.1) wiederhergestellt, alle 7 Testsuiten liefen danach
weiterhin fehlerfrei (0 Regressionen).

## [0.14.1] — Objekt-Rotation korrigiert (.nif-Achsentausch), Textur-Spiegelung: begründeter Fix

### Korrigiert
- **`.nif`-Vertex-Positionen und -Normalen fehlte das Z-up→Y-up-Achsen-Remap**, das für den
  Rest des Legacy-Formats bereits gilt (siehe `ObjectPlacementIO.cpp`) - dadurch standen
  platzierte Objekte in falscher Rotation. Gleiches Remap jetzt auch für `.nif`-Meshdaten
  angewendet (Achsentausch Y↔Z). Reine Umsortierung, keine Byte-Anzahl-Änderung - alle
  bisherigen Verifikationen bleiben gültig (Massentest weiterhin 257/3436)

### Vermutlich behoben, aber nicht verifiziert (kein Screenshot verfügbar)
- **Diffuse-Textur-Spiegelung**: V-Komponente der Diffuse-Textur-Koordinate im Terrain-Shader
  gespiegelt - wahrscheinlichste Ursache ist eine DirectX/OpenGL-Konventionsdifferenz für die
  vertikale Textur-Achse (Original-Engine vermutlich DirectX-basiert). Bewusst nur die
  Diffuse-Textur betroffen, nicht die Blend-Gewichts-Textur (deren Ausrichtung war bereits
  validiert). Ohne Screenshot nicht abschließend verifizierbar - falls die Karte weiterhin
  falsch orientiert erscheint, liegt die Ursache woanders

## [0.14.0] — .nif: Material/Texturing-Basis korrigiert (257 von 3436 Dateien ladbar, vorher 17)

### Korrigiert
- **`NiTexturingProperty`**: volle 12-Byte-`ObjectNetBase` statt der zuvor angenommenen
  verkürzten 8-Byte-Version - fiel erst bei einer dritten, strukturell anderen Vergleichsdatei
  (`R_Helga01GL.nif`) auf, da sich dieser Fehler bei den ersten beiden Testdateien zufällig mit
  einem zweiten Fehler kompensierte
- **`NiMaterialProperty`**: optionales 15. Float (0.0), abhängig davon, ob das zugehörige
  `NiTriStrips` eine `NiTexturingProperty` referenziert (untexturiert → 15 Floats, texturiert →
  14) - an 4 echten Dateien byte-exakt bis Dateiende verifiziert
- **`NiPixelData`**: konditionaler 8-Byte-Trailer, nur vorhanden wenn NICHT direkt eine weitere
  `NiSourceTexture` folgt
- **`NiAlphaProperty`** (15 Byte) und **`NiStencilProperty`** (vorläufige Länge) ergänzt -
  `NiAlphaProperty` blockierte allein 1989 der 3436 echten Dateien
- **Performance-Fix**: Zählfelder hatten zu großzügige Sicherheitsgrenzen (2 Mio.), wodurch
  fehlausgerichtete Dateien sehr langsam (nicht unendlich, aber spürbar träge) verarbeitet
  wurden. Kontextspezifische, deutlich engere Grenzen ergänzt (z.B. Vertex-Anzahl max. 200.000)
  - Massentest über alle 3436 Dateien läuft jetzt in unter 90 Sekunden

### Ergebnis
**Massentest über alle 3436 echten `.nif`-Dateien: 257 laden mit vollständiger Geometrie
(vorher 17, davor 166 in einem Zwischenschritt)** - 0 Abstürze, alle nicht unterstützten Fälle
scheitern weiterhin sauber mit Fehlermeldung. `NiStencilProperty` ist jetzt der größte
verbleibende Blocker (~270 Dateien) und noch nicht verifiziert (Platzhalter-Länge).

## [0.13.2] — NiTexturingProperty/NiSourceTexture/NiPixelData-Kette fast lückenlos nachvollzogen

### Hinzugefügt (Dokumentation, noch nicht in Code umgesetzt)
- `NiTexturingProperty`: `apply_mode`, `texture_count`, 7 Textur-Slots mit `TexDesc`-Struktur
  vollständig nachvollzogen - **beide belegten `source_ref`-Werte zeigen exakt auf die laut
  Block-Typ-Liste erwarteten `NiSourceTexture`-Blöcke**
- `NiSourceTexture`: Dateiname-Position exakt verifiziert (`"top_wall_c256.dds"`),
  `pixel_data`-Referenz **verifiziert: zeigt exakt auf den erwarteten `NiPixelData`-Block**
- `NiPixelData`: Größenfeld für die eingebettete Rohpixel-Daten (vermutlich ein Asset-Browser-
  Thumbnail, da die echte Textur separat als `.dds` vorliegt) empirisch lokalisiert - die
  gesamte Kette `NiTexturingProperty → NiSourceTexture(Datei 1) → NiPixelData(43704 Byte) →
  NiSourceTexture(Datei 2)` trifft **exakt** auf den unabhängig gefundenen zweiten
  Textur-Dateinamen
- **Einziges verbleibendes Puzzlestück:** der variable Mipmap-Header VOR diesem Größenfeld
  (bei der untersuchten Datei 175 Byte, vermutlich abhängig von der Mipmap-Anzahl) ist noch
  nicht feldweise entschlüsselt - bräuchte eine zweite Vergleichsdatei mit abweichender
  Mipmap-Anzahl (analog zur `.shbd`-Methode), um robust zu werden. Ohne das bleibt
  `LoadNifMesh` bei texturierten Meshes weiterhin auf "nicht unterstützt", auch wenn die
  Struktur jetzt fast vollständig verstanden ist

## [0.13.1] — Format-Übersicht dokumentiert, NiTexturingProperty teilweise entschlüsselt

### Hinzugefügt
- Neue Übersichtstabelle am Anfang von `docs/MAP_FORMAT.md`: alle Dateiformate (`.tshm`, `.HTD`,
  `.shbd`, `.shmd`, `.idm`/`.aid`, `.dds`, `.nif`, `.sbi` u. a.) mit Status und Verweis auf den
  jeweiligen Detailabschnitt
- **`NiTexturingProperty`-Struktur positionsgenau nachvollzogen** (noch nicht in Code
  umgesetzt): kurze Basis, `apply_mode`, `texture_count`, 7 Textur-Slots mit `TexDesc`
  (Quell-Referenz + Clamp/Filter-Modus + UV-Set + optionale Transform) - **die berechnete
  Position landet exakt auf dem unabhängig gefundenen Textur-Dateinamen**, und beide belegten
  `source_ref`-Werte zeigen exakt auf die laut Block-Typ-Liste erwarteten
  `NiSourceTexture`-Blöcke. `NiSourceTexture`/`NiPixelData` selbst noch offen - texturierte
  Meshes bleiben vorerst nicht ladbar

## [0.13.0] — Echte Meshes in 3D gerendert + texturierte 2D-Draufsicht (Heightmap/Texturing/Walk)

### Hinzugefügt
- **`NifMeshRenderer`**: lädt und rendert echte `.nif`-Geometrie für alle Objekte, bei denen
  `core::LoadNifMesh` erfolgreich ist (aktuell 17 von 3436 echten Dateien, siehe v0.12.0) -
  einfacher Lambert-Shader mit der extrahierten Materialfarbe, kein Instancing (jedes Modell hat
  eigene Geometrie). Wird automatisch beim Öffnen einer Karte sowie bei den granularen
  `.tsobj`/`.shmd`-Objekt-Importen neu geladen. `ObjectMarkerRenderer` zeichnet die
  Platzhalter-Pyramide jetzt NUR NOCH für Objekte, für die kein echtes Mesh geladen werden
  konnte (kein doppeltes Zeichnen mehr)
- **Texturierte Draufsicht für den 2D-Editor**: `HeightmapRenderer::BeginTopDownScene` nutzt
  denselben Multi-Layer-Diffuse-Blend-Shader wie die 3D-Ansicht, nur mit einer orthographischen
  Kamera direkt von oben (`OrthoTopDownViewProj`, neu in `Camera.cpp`) - Heightmap-, Texturing-
  und Objekt-Placement-Modus zeigen jetzt die echte, alle Layer bereits zusammengemischte
  Kartentextur statt einer reinen Graustufen-Vorschau
- **Block&Walk-Overlay**: `HeightmapRenderer::DrawTopDownOverlay` legt die Block&Walk-Heatmap
  halbtransparent rot (Deckkraft nach Heat-Wert) über die echte Kartentextur, statt sie isoliert
  in Graustufen anzuzeigen - eigener zweiter Framebuffer (`fbo2d_`, unabhängig von der
  3D-Vorschau) und ein einfacher Vollbild-Quad-Shader für die Überlagerung

### Bekannte Einschränkungen (v0.13.0)
- Nicht mit echtem GLFW/ImGui/glad gegenkompiliert (wie der Rest der App-Schicht) - nur
  syntaktisch gegen einen erweiterten GL-Stub geprüft
- Overlay-Ausrichtung (welche Bildseite welcher WalkGrid-Zeile entspricht) ist über sorgfältige
  Herleitung der OpenGL-Textur-Konventionen bestimmt, aber nicht visuell verifizierbar - falls
  die Block&Walk-Heatmap nach dem Bauen vertikal gespiegelt zur Kartentextur erscheint, muss nur
  die uv.y-Berechnung im `quadVerts`-Array (Renderer.cpp) umgekehrt werden
- `previewTex`/`layerPreviewTex` (die alten Graustufen-Texturen) werden weiterhin berechnet, aber
  nicht mehr angezeigt - könnten in einem Aufräum-Durchgang entfernt werden

## [0.12.0] — `.nif`-Parser: echte Geometrie für untexturierte Meshes extrahierbar

### Hinzugefügt
- `core::LoadNifMesh` (`NifModel.hpp`/`.cpp`): GUI-freier Parser für Gamebryo/NetImmerse-Dateien
  (Version 20.0.0.4). Verifiziert per Byte-exaktem Dateiende-Abgleich an 2 echten Dateien
  unterschiedlicher Vertex-/Dreieckszahl (`Eld_CD.nif`, `AddSharpCD.nif`) - komplette Struktur
  für Datei-Header, `NiNode`-Szenengraph, `NiMaterialProperty` und das `NiTriStrips`/
  `NiTriStripsData`-Paar (Vertices, Normalen, Vertexfarben, UVs, Dreiecksstreifen) entschlüsselt
  und in echten C++-Code überführt. Details und Herleitung siehe docs/MAP_FORMAT.md
- Sicherheitsgrenzen (`CountU32`/`CountU16`) gegen `bad_alloc`-Abstürze bei unerwarteten/nicht
  unterstützten Block-Strukturen ergänzt - der Parser bricht bei einem nicht unterstützten
  Block-Typ (z. B. `NiTexturingProperty`) kontrolliert mit Fehlermeldung ab, statt zu
  versuchen, riesige Fake-Arraygrößen zu allozieren
- **Massentest über alle 3436 echten `.nif`-Dateien** aus den bereitgestellten Kartensets: 17
  laden mit vollständiger, korrekter Geometrie (0 Abstürze bei den restlichen 3419, alle mit
  klarer Fehlermeldung statt Absturz oder falschen Daten)
- 8 neue Tests in `tests/test_nif_model.cpp` (Gesamt: 108 Checks über 6 statische + 2
  parametrisierte Testdateien)

### Bekannte Einschränkungen (v0.12.0)
- **Texturierte Meshes werden noch nicht unterstützt** (die große Mehrheit der echten Dateien) -
  `NiTexturingProperty`/`NiSourceTexture`-Struktur noch nicht entschlüsselt (Textur-Dateiname
  wurde per Landmarken-Suche gefunden, aber die ca. 74 Byte davor noch nicht vollständig
  aufgeschlüsselt)
- Kein GL-Rendering der extrahierten Geometrie integriert - `ObjectMarkerRenderer` zeigt
  weiterhin nur Platzhalter-Marker, auch für Objekte mit erfolgreich geladener echter Geometrie.
  Das ist der nächste konkrete Schritt für "echte Objekte in der 3D-Vorschau"
- Weitere unbekannte Block-Typen (`NiCollisionData`, `NiStringExtraData`, `NiPointLight`,
  `NiTriShape`/`NiTriShapeData` u. a.) nicht unterstützt

## [0.11.1] — 2D-Editor zeigt Rohgitter jetzt immer in Heightmap-Form an

### Korrigiert
- Nutzer-Anforderung: Block&Walk (und Texturing) sollen auf einer Fläche bearbeitet werden, die
  wie die 2D-Heightmap aussieht - nicht in der nativen (bei Block&Walk extrem länglichen, 1:16)
  Pixel-Form des Rohgitters. `DrawEditor2D` bemisst die Anzeigefläche jetzt immer anhand der
  Heightmap-Ausdehnung statt anhand der jeweiligen Rohgitter-Dimensionen - die Rohdaten werden
  beim Zeichnen automatisch auf diese Form gestreckt/gestaucht (wie eine Textur auf ein
  andersförmiges Quad)
- **Kein Save-seitiger Konvertierungscode nötig:** die Mal-Logik (Weltposition → Rasterzelle)
  rechnete bereits vorher korrekt über die tatsächliche Kartenausdehnung um, unabhängig von der
  Anzeigegröße - nur die Anzeige selbst war falsch bemessen. `ExportLegacyShbd` schreibt
  weiterhin exakt die unveränderte Rohform zurück (byte-exakt, unverändert getestet)
- Als Nebeneffekt auch für Texturing konsistent: unabhängig auflösende Textur-Layer werden jetzt
  ebenfalls immer in Heightmap-Form angezeigt statt in ihrer eigenen (meist quadratischen, aber
  nicht notwendig heightmap-proportionalen) nativen Form

## [0.11.0] — Block&Walk-Gitter-Auflösung endgültig korrigiert (vom Nutzer per Screenshot gefunden)

### Korrigiert — wichtigster Fund seit Projektbeginn
- **Block&Walk-Gitter war NICHT quadratisch**, wie seit v0.4.0 angenommen. Der Nutzer bemerkte
  im laufenden Tool eine sichtbare 4-fache Wiederholung derselben Silhouette im 2D-Editor
  (Screenshot). Direkte Visualisierung der rohen `.shbd`-Bytes bei verschiedenen Kandidaten-
  Breiten (Python, unabhängig vom Tool) bestätigte: **Breite = QuadsBreite/2, Höhe =
  QuadsBreite×8** (Verhältnis exakt 1:16) - nicht `max(QuadsBreite,QuadsHöhe)×2` (quadratisch)
  wie zuvor angenommen. **Wichtige methodische Lehre:** Der bisherige Byte-für-Byte-Roundtrip-
  Test verifizierte nur die GESAMT-Byte-Anzahl (512×512 = 128×2048 = 262144 Elemente in beiden
  Fällen), nicht die tatsächliche Breite/Höhe-Aufteilung - ein Fehler in der 2D-Interpretation
  konnte dadurch unentdeckt bleiben, obwohl alle Roundtrip-Tests durchgehend grün waren. Erst
  visuelle Kontrolle deckte es auf
- `core::PeekLegacyShbdHeader` (neu): liest die tatsächliche Gitterhöhe direkt aus dem
  Datei-Header (zweites Feld, an allen 4 Karten exakt bestätigt) statt sie aus einer Formel
  herzuleiten - robuster, da selbstbeschreibend. `OpenLegacyMap` nutzt das jetzt bevorzugt,
  mit der Formel als Fallback
- `SyncWalkGridSize` (main.cpp, für neue native Karten) und `OpenLegacyMap` (Legacy-Import)
  beide korrigiert. `tests/test_walk_grid.cpp` von hartkodierten 512×512 auf die korrekten
  128×2048 aktualisiert (bestand vorher nur zufällig, weil beide Werte dieselbe Gesamt-
  Elementanzahl ergeben)
- **Verifiziert an allen 4 echten Kartensets** (Rou/Bera: 128×2048, RouVal01/Eld: 256×4096,
  Adl: 475×7600 - Verhältnis überall exakt 16.0): korrekte, nicht wiederholte Interpretation
  bestätigt, UND weiterhin byte-exakter Export-Roundtrip erhalten

### Bekannte Einschränkungen (v0.11.0)
- Die genaue Achsen-Zuordnung (welche der beiden stark unterschiedlichen Dimensionen der Welt-
  X- bzw. -Z-Achse entspricht) bleibt ungeklärt - betrifft nur die Orientierung der Anzeige/des
  Pinsels, nicht die Datenkorrektheit
- `.nif`-Objektgeometrie-Parsing (für echte Meshes statt Platzhalter-Marker) weiterhin in
  Arbeit - Datei-Header und Szenengraph-Traversierung (NiNode-Hierarchie) sind verifiziert,
  die eigentlichen Geometrie-Blöcke (Vertex-/Dreiecksdaten) noch nicht vollständig entschlüsselt

## [0.10.0] — Echte Diffuse-Texturen im 3D-Terrain (DDS/BC1-BC3 + Multi-Layer-Blend-Shader)

### Hinzugefügt
- `core::DdsImage` (`LoadDdsImage`): GUI-freier DDS-Loader, dekodiert BC1/DXT1, BC2/DXT3 und
  BC3/DXT5 (die einzigen in den 160 echten Field-Texturen vorkommenden Formate, empirisch
  geprüft) zu rohem RGBA8. **Verifiziert gegen eine unabhängige Referenzimplementierung**
  (Pillow/libImaging) an je einer echten DXT1-, DXT3- und DXT5-Datei: max. Abweichung 1/255 pro
  Kanal (Rundungsdifferenz bei der RGB565→888-Konvertierung, keine strukturelle Abweichung)
- `HeightmapRenderer::LoadTerrainTextures`/`UpdateBlendTextures`: lädt bis zu 8 echte
  Diffuse-Texturen + deren Blend-Gewichte und rendert sie im 3D-Terrain gemischt, statt des
  bisherigen reinen Höhen-Farbverlaufs (der als Fallback erhalten bleibt, z.B. direkt nach
  "Neu"). Diffuse-Pfade werden über dieselbe Mehrfach-Wurzel-Pfadauflösung wie die Blend-BMPs
  aufgelöst; nicht ladbare Layer bekommen eine graue Platzhalter-Textur statt den Aufbau
  abzubrechen
- Fragment-Shader nutzt 8 fest benannte Sampler-Uniform-Paare (Diffuse+Blend) statt eines
  Sampler-Arrays - dynamische Array-Indizierung von Samplern ist im strikten GLSL-330-Core-
  Profil nicht garantiert, feste Uniforms sind auf jeder GL-3.3-Hardware sicher
- Textur-Malen aktualisiert die 3D-Ansicht jetzt live (Blend-Gewichte werden bei jedem
  Pinselstrich und bei Undo/Redo neu hochgeladen, ohne die Diffuse-DDS neu zu laden)

### Bekannte Einschränkungen (v0.10.0)
- **2D-Editor zeigt weiterhin nur Graustufen** (Höhe bzw. einzelnes Layer-Gewicht), keine echte
  Textur-Komposition - eigener Folgeschritt (z.B. über eine zusätzliche orthographische
  Top-Down-Render-Passage mit demselben Shader), noch nicht umgesetzt
- Karten mit mehr als 8 Textur-Layern (z.B. `Teva` mit 13) zeigen nur die ersten 8 texturiert -
  Grenze durch garantierte Mindestanzahl an Textur-Units in GL 3.3 (16, davon 2 pro Layer)
- Bedeutung von `UVScaleDiffuse` aus der Legacy-`.ini` nicht abschließend gesichert - aktuelle
  Kachelung (alle 500 Welteinheiten, skaliert mit diesem Wert) ist eine plausible Annahme, nicht
  gegen das Original-Rendering verifizierbar
- Renderer-Änderungen syntaktisch gegen erweiterten GL-Stub geprüft (kompiliert sauber), aber
  wie der Rest der App-Schicht nicht mit echtem GLFW/ImGui/glad gegenkompiliert

## [0.9.0] — Nutzer-Feedback aus erstem echten Testlauf: Map-Öffnen-Fix, Rot-Tint-Fix, bewegliche Kamera

### Korrigiert
- **`.HTD`-Import scheiterte bei mehreren echten Karten** (`BigCoast.HTD`/`UrgDark01.HTD`/
  `UrgSwa01.HTD`), weil diese zusätzliche Daten nach dem reinen Höhenraster enthalten
  (4 bis 5268 Byte, Bedeutung nicht gesichert). `ImportLegacyHtd`/`ExportLegacyHtd` erfassen und
  reproduzieren diese Bytes jetzt statt den Import abzubrechen - **verifiziert an allen drei
  echten Dateien: Import erfolgreich UND Re-Export weiterhin byte-exakt**. Ergebnis am realen
  29-Karten-Set: 26 von 29 laden jetzt vollständig (vorher 23 von 29) - die 3 verbleibenden sind
  Datenvollständigkeits-/Formatfragen, keine Code-Bugs (siehe docs/MAP_FORMAT.md)
- **Graustufen-Vorschauen erschienen ROT statt grau** (Heightmap-, Textur-Layer- und
  Block&Walk-Vorschau im 2D-Editor): fehlender Textur-Swizzle bei `GL_R8`-Einkanaltexturen -
  OpenGL liest G/B-Kanäle ohne expliziten Swizzle als 0, wodurch nur der Rot-Kanal Werte trägt.
  `GL_TEXTURE_SWIZZLE_G/B/A` ergänzt an allen drei Stellen

### Hinzugefügt
- **Bewegliche 3D-Kamera**: rechte Maustaste + ziehen verschiebt jetzt das Kamera-Zielzentrum
  (Pan), skaliert mit der aktuellen Kameraentfernung; vorher war nur Rotation um einen fest bei
  (0,0,0) verankerten Punkt möglich
- "Kamera zentrieren"-Button im Werkzeuge-Panel; Kamera wird beim Öffnen einer Karte jetzt
  automatisch auf deren Mittelpunkt zentriert und die Entfernung an die Kartengröße angepasst
  (vorher: fester Startpunkt/Zoom unabhängig von der tatsächlichen Kartengröße)

### Bekannte Einschränkungen (v0.9.0) / als Nächstes
- Echte Diffuse-/Blend-Texturen werden in 2D UND 3D weiterhin nicht angezeigt (nur Graustufen
  bzw. prozeduraler Höhen-Farbverlauf) - als Nächstes geplant. Diffuse-Texturen sind DXT1-
  komprimierte `.dds`-Dateien (256x256, mit Mipmaps, per Header-Analyse bestätigt) - eigener
  BC1-Decoder + Mehrlagen-Blend-Shader nötig, noch nicht begonnen
- Objekte zeigen weiterhin nur Platzhalter-Marker (Pyramide+Pfeil), keine echten `.nif`-Meshes
- Offene Rückfrage vom Nutzer ("Block&Walk muss auf ein Segment gematcht werden nicht 4") noch
  nicht umgesetzt - Bedeutung nicht abschließend geklärt

## [0.8.1] — Fix: windows.h min/max-Makro-Kollision mit std::max/std::min

### Korrigiert
- `<windows.h>` wurde ohne `NOMINMAX` eingebunden - windows.h definiert dadurch `min`/`max` als
  Präprozessor-Makros, die jedes `std::max(...)`/`std::min(...)` im restlichen `main.cpp` kaputt
  machen (MSVC-Fehler `C2589`, "ungültiges Token rechts von ::"). `#define NOMINMAX` vor dem
  Include ergänzt - klassischer, gut bekannter Windows-Header-Stolperstein

## [0.8.0] — Nativer Ordner-Browser + automatische Kartenerkennung

### Hinzugefügt
- "Asset-Ordner wählen..."-Button im Datei-Menü öffnet einen nativen Windows-Ordnerdialog
  (`SHBrowseForFolder`, Windows SDK, keine zusätzliche Abhängigkeit) - Nutzer muss keine Pfade
  mehr von Hand eintippen
- Automatischer Scan (`ScanForMaps`, begrenzte Rekursionstiefe) findet alle `.ini`-Dateien unter
  dem gewählten Ordner und listet sie als anklickbare Kartenliste; steigt bewusst NICHT in
  `fieldTexture/`-Ordner ab (dort liegen nur geteilte Texturen, keine Karten, aber potenziell
  viele Dateien) - Auswahl aus der Liste befüllt automatisch das "Karte öffnen"-Feld
- Gleicher Ordnerdialog auch für das Speichern-Ausgabeverzeichnis verfügbar
- **Scan-Logik isoliert (ohne GL/ImGui-Abhängigkeit) gegen die echten Kartenordner getestet:**
  findet alle 4 echten Karten (Adl/Bera/Eld/RouVal01) korrekt, überspringt `fieldTexture`
  zuverlässig

### Bekannte Einschränkungen (v0.8.0)
- Ordnerdialog ist Windows-only (`#ifdef _WIN32`) - unter Linux weiterhin nur manuelle
  Pfadeingabe (das Textfeld bleibt in jedem Fall sichtbar/nutzbar, auch unter Windows)
- Ältere `SHBrowseForFolder`-API statt der moderneren `IFileOpenDialog`-COM-API gewählt (weniger
  fehleranfällig ohne lokale Kompilierbarkeit, dafür optisch schlichter - kein "Neuer Ordner"-
  Button im Dialog)
- Scan-Logik selbst getestet, aber `BrowseForFolderWindows` (der eigentliche Win32-Dialog-Aufruf)
  naturgemäß nicht - das ist der Teil, der jetzt einen echten Windows-Testlauf braucht

## [0.7.3] — Zwei echte main.cpp-Bugs aus dem ersten GUI-Compile-Versuch behoben

### Korrigiert (gefunden durch den ersten echten main.cpp-Compile mit MSVC - vorher nie kompiliert,
nur die reinen GL-Dateien Renderer.cpp/ObjectMarkerRenderer.cpp hatte ich mit einem Stub geprüft)
- `EditorState::legacySpatialIndex` war fälschlich als `core::legacy::ObjectSpatialIndex`
  deklariert - der Typ liegt aber in `core::ObjectSpatialIndex` (ohne `::legacy::`). Tippfehler
  beim Bau des Objekt-Placement-Panels
- `EditorState::objectPlaceMode` war als `bool` deklariert, wurde aber mit
  `ImGui::RadioButton(const char*, int*, int)` verwendet, das einen `int*` erwartet - auf `int`
  umgestellt (1 = Platzieren, 0 = Auswählen)

### Bekannte Einschränkungen (v0.7.3)
- Nach diesen zwei Fixes noch kein erneuter Build-Versuch bestätigt - da main.cpp vorher nie
  kompiliert wurde, sind weitere kleinere Fehler dieser Art (Namespace-Tippfehler, ImGui-
  Overload-Mismatches) beim nächsten Versuch nicht auszuschließen

## [0.7.2] — Erster echter Windows-Build (MSVC): Core + alle Tests kompilieren, ein GUI-Fix

### Bestätigt (echter Windows/MSVC-Build durch den Nutzer, nicht mehr nur Syntax-Stub)
- `mapeditor_core.lib` UND alle 6 Test-Executables kompilieren und linken sauber mit MSVC
  (Visual Studio 2022, `/std:c++latest`) - erste echte Bestätigung, dass `std::expected` und der
  gesamte C++23-Code cross-platform funktionieren, nicht nur unter GCC/Linux

### Korrigiert
- `main.cpp` inkludierte die Dear-ImGui-Backend-Header über `backends/imgui_impl_glfw.h` /
  `backends/imgui_impl_opengl3.h` (Layout des offiziellen Dear-ImGui-Repos). Der vcpkg-`imgui`-
  Port installiert diese Header aber OHNE `backends/`-Unterordner direkt ins Include-Verzeichnis.
  Includes entsprechend angepasst (kein `backends/`-Präfix mehr)

### Bekannte Einschränkungen (v0.7.2)
- `main.cpp`/`Renderer.cpp`/`ObjectMarkerRenderer.cpp`/`Camera.cpp` nach diesem Fix noch nicht
  erneut gegenkompiliert (wartet auf Rückmeldung vom nächsten Windows-Build-Versuch) - weitere
  kleinere ImGui-API-Abweichungen sind beim ersten echten GUI-Build nicht auszuschließen

## [0.7.1] — Fix: inkonsistente Blend-Auflösung innerhalb einer Karte (Adl)

### Korrigiert
- `ImportLegacyTextureSet` verwarf bisher Textur-Layer, deren Blend-BMP von der für die Karte
  erkannten Auflösung abwich (bei der echten Adl-Karte: 8 von 10 Layern, 476×476 statt 512×512).
  Neue `core::ResampleBlendMap` (bilinear) resampelt solche Layer jetzt auf die Stack-Auflösung,
  statt sie ohne Daten zu lassen. **Verifiziert: alle 10 von 10 Adl-Layer haben jetzt echte
  Blend-Daten** (vorher 2 von 10). Vollständiger Open→Save→Reopen-Rundlauf weiterhin fehlerfrei
  für Adl/Bera/RouVal01

### Bekannte Einschränkungen (v0.7.1)
- Resampling ist nicht verlustfrei (nur relevant bei starkem Auflösungsunterschied zwischen
  Layern derselben Karte - bei Adl 476→512, ein moderater Unterschied)
- `Eld` weiterhin nicht öffenbar (anderes Format) - laut Nutzer vermutlich Gamebryo-bezogen,
  keine Dokumentation dazu verfügbar; ohne weitere `.sbi`-Referenzdateien nicht weiter
  aufschlüsselbar (anders als bei `.shbd`, wo der Vergleich über mehrere echte Karten
  unterschiedlicher Form die entscheidende Erkenntnis brachte)

## [0.7.0] — Vereinheitlichter "Karte öffnen/speichern"-Workflow

### Hinzugefügt
- `core::legacy::LegacyMapProject` + `OpenLegacyMap`/`SaveLegacyMap`: öffnet/speichert eine
  komplette Legacy-Karte (Heightmap + Texturing + Block&Walk + Objekt-Placement + räumlicher
  Index + Zonen-Metadaten) über EINE Aktion statt sieben Einzel-Importen. Fehlende/nicht ladbare
  Teile sind nicht fatal (Report statt Abbruch) - nur die `.ini` selbst ist zwingend
- `core::legacy::LegacyPathResolve`: gemeinsame Pfad-Resolver-Utilities aus
  `LegacyTextureSetIO.cpp` herausgelöst (keine Code-Duplikate mehr), plus neue
  `FindSiblingFileByStem` für die Begleitdatei-Auffindung
- GUI: neuer, prominent platzierter "Karte öffnen/speichern"-Bereich ganz oben im Datei-Menü;
  bisherige Einzel-Modul-Importe bleiben als "fortgeschritten" darunter erhalten
- 15 neue Tests in `tests/test_legacy_map_project.cpp` (voller Open→Save→Reopen-Rundlauf,
  parametrisiert über Kommandozeilenargument - läuft gegen Bera/Adl/RouVal01 mit identischem
  Ergebnis: Heightmap byte-exakt, alle Modul-Dimensionen und Objekt-Anzahl nach Rundlauf
  identisch). Gesamt: 100 Checks über 5 statische + 1 parametrisierten Test

### Korrigiert
- Granulare `.HTD`/`.shbd`-Import-Buttons lasen den Legacy-Header bisher NICHT ein (nutzten beim
  Export immer den Default-Header statt des tatsächlich importierten) - jetzt wird der Header
  beim Import erfasst und beim Export wiederverwendet, für byte-exakten Re-Export auch im
  granularen Workflow

### Bekannte Einschränkungen (v0.7.0)
- RouVal01 hat keine `.aid`-Datei (Zonen-Metadaten) - wird als "nicht vorhanden" behandelt, kein
  Fehler
- Adl hat inkonsistente Blend-Auflösung zwischen Layern derselben Karte (siehe v0.6.0/
  docs/MAP_FORMAT.md) - 8 von 10 Textur-Layern laden dort keine echten Blend-Daten
- `Eld` weiterhin nicht öffenbar (anderes Format, siehe v0.4.0)
- GUI-Code (`main.cpp`) weiterhin nicht gegen echte GLFW/ImGui/glad-Bibliotheken kompiliert
  (siehe v0.1.0/v0.6.0) - Core-Workflow (`OpenLegacyMap`/`SaveLegacyMap`) dagegen vollständig
  kompiliert und gegen echte Kartensets getestet

## [0.6.0] — Textur-Auflösung entkoppelt, Legacy-Pfadauflösung repariert, Windows-Härtung

### Korrigiert — drei zusammenhängende Funde anhand der echten Kartensets
- **Textur-Layer-Auflösung war fälschlich an die Heightmap gekoppelt** (siehe v0.4.0-Fund).
  `TextureLayerStack` nutzt jetzt eine unabhängige Auflösung; `ImportLegacyTextureSet` bestimmt
  sie aus der ERSTEN tatsächlich ladbaren Blend-BMP statt sie zu erzwingen; Fallback 512x512
  (dokumentierter Platzhalter) falls keine BMP lesbar ist. `ExportLegacyTextureSet` überschreibt
  die Heightmap-Ini-Felder nicht mehr fälschlich mit der Textur-Auflösung. GUI-Pinsel-Mapping
  (Texturing UND Zuordnung im 2D-Editor) entsprechend korrigiert
- **Legacy-Blend-Pfade waren nicht auflösbar:** echte Karten referenzieren Blend-BMPs relativ zu
  einer GETEILTEN Asset-Wurzel (z.B. `fieldTexture/` als Geschwister-Ordner von `field/<Karte>/`),
  nicht relativ zum Kartenordner selbst; zusätzlich sind die Pfade case-sensitiv falsch
  (Windows-authored, `moss.bmp` vs. echte Datei `Moss.BMP`). Neuer Resolver
  (`ResolveLegacyAssetPath`) probiert mehrere plausible Wurzeln mit case-insensitivem Fallback
  pro Pfad-Komponente. Das virtuelle `resmap`-Präfix (kein echter Ordner) wird jetzt konsistent
  von Import UND Export entfernt (vorher nur beim Import - Export schrieb an eine andere Stelle,
  als der Import erwartete). **Verifiziert: voller Import→Export→Reimport-Rundlauf mit den
  echten Bera-Daten (10 Layer, 512×512) - 0.0 Abweichung, 0 fehlende Dateien**
- **Windows-Härtung** (Nutzer-Anforderung: Editor muss am Ende auf Windows laufen): `strnlen`
  (zwar auch unter MSVC vorhanden, aber POSIX-Herkunft) durch reine Standardbibliotheks-Variante
  ersetzt; `/utf-8`-Compiler-Flag für MSVC ergänzt (`main.cpp` enthält direkt eingebettete
  deutsche Umlaute in ImGui-Labels, MSVC würde diese sonst nach Systemcodepage statt UTF-8
  interpretieren); README um expliziten Windows-Build-Abschnitt inkl. Compiler-Versionsanforderung
  (`std::expected` braucht VS2022 17.9+) ergänzt

### Bekannte Einschränkungen (v0.6.0)
- Windows-Build weiterhin nicht tatsächlich kompiliert (kein Windows-Rechner in dieser Umgebung
  verfügbar) - Code wurde aber gezielt auf bekannte Windows-Stolpersteine durchsucht und
  gehärtet, siehe README.md
- `.nif`-Import, `Eld`/`.sbi`-Format weiterhin offen (siehe v0.4.0/v0.5.0)

## [0.5.0] — Heightmap + Objekt-Placement gemeinsam im 3D-Preview

### Hinzugefügt
- `app::ObjectMarkerRenderer`: GPU-instanced Platzhalter-Marker (Pyramide + Richtungspfeil) für
  jedes platzierte Objekt im 3D-Preview - zeigt Position, Blickrichtung (Rotation um die
  Hochachse) und Skalierung, ausgewähltes Objekt farblich hervorgehoben. Kein echtes
  `.nif`-Mesh-Rendering (eigenes, größeres Folgeprojekt, siehe docs/MAP_FORMAT.md) - Zweck ist,
  Layout/Dichte der Objekt-Placements im Kontext des Terrains einschätzen zu können
- `HeightmapRenderer::BeginScene`/`EndScene`: Terrain- und Objekt-Rendering teilen sich jetzt
  einen gemeinsamen Framebuffer-Pass (korrekter Depth-Test zwischen beiden)

### Korrigiert — wichtiger Fund
- **Achsen-Konvention:** `Rou.shmd` (und vermutlich alle Legacy-Objektdaten) sind **Z-up**
  (X/Y = horizontale Ebene, Z = Höhe), das Heightmap-Modul ist **Y-up**. Verifiziert durch
  Abgleich echter Objekt-Positionen gegen `Heightmap.SampleWorld()` (Diff nahe 0 für
  bodenstehende Objekte vor dem Fix bei falscher Achse, nach dem Fix bei korrekter). Ohne
  diesen Fix wären Objekte beim Rendern um die komplette Kartenhöhe (mehrere Hundert bis
  Tausend Einheiten) falsch platziert gewesen. Fix sitzt an der Legacy-Import/-Export-Grenze
  (`ParseLegacyShmd`/`SerializeLegacyShmd` tauschen Y/Z für Position UND Rotation) - reines
  Vertauschen ohne Berechnung, `.shmd`-Byte-Exaktheit bleibt dadurch erhalten (erneut gegen
  `Rou.shmd` verifiziert). GUI-Rotationsfeld entsprechend von "Rotation Z" auf "Rotation um
  Hochachse" umbenannt (liegt jetzt korrekt auf `rotY` statt `rotZ`)

### Bekannte Einschränkungen (v0.5.0)
- Objekt-Marker sind Platzhalter (keine echten Meshes) - `.nif`-Import bleibt offen
- Achsen-Fix nur für reine Yaw-Rotation verifiziert (siehe v0.4.0-Eintrag zu zusammengesetzten
  Rotationen)
- `ObjectMarkerRenderer.cpp`/`Renderer.cpp` sind syntaktisch gegen einen minimalen GL-Funktions-
  Stub geprüft (kompiliert sauber), aber wie der Rest der App-Schicht nicht mit echten
  GLFW/ImGui/glad-Bibliotheken gegenkompiliert - siehe v0.1.0-Hinweis

## [0.4.0] — Phase 4: Objekt-Placement-Modul + Validierung an 4 zusätzlichen echten Karten

### Hinzugefügt
- `core::ObjectPlacementSet`: Kategorie-Listen, Szene-Umgebung (Licht/Nebel/Hintergrund), flache
  Instanzliste (Modellpfad + Position + Quaternion-Rotation + Scale)
- `core::ObjectPlacementIO`: natives `.tsobj`-Format + `legacy::ParseLegacyShmd`/
  `SerializeLegacyShmd` — **byte-für-byte identisch** zur echten `Rou.shmd` (inkl. exakter
  Fließkomma-Formatierung, siehe "Korrigiert")
- `core::ObjectSpatialIndex` + `legacy::ParseLegacyIdm`/`SerializeLegacyIdm` — **byte-für-byte
  identisch** zur echten `Rou.idm` (1178 variabler-Länge-Gruppen, Struktur vollständig verifiziert)
- `legacy::ParseLegacyAid`/`SerializeLegacyAid` — **byte-für-byte identisch** zur echten `Rou.aid`
  (inkl. nicht genullter Speicherreste im Namensfeld, roh durchgereicht statt rekonstruiert)
- GUI: vierter Editor-Modus "Objekte" (Platzieren/Auswählen per Klick, Marker-Overlay, Rotation/
  Skalierung editierbar), Menü um Objekt-Placement/idm/aid-Import-Export ergänzt
- 22 neue Tests in `tests/test_object_placement.cpp` (Gesamt: 78 Checks über fünf Testdateien)

### Korrigiert (anhand von 4 zusätzlichen echten Kartensets: Adl, Bera, Eld, RouVal01)
- **BMP-Format:** Blend-Bitmaps sind real **24-bit RGB**, nicht 8-bit indiziert wie in v0.3.0
  angenommen — Codec umgeschrieben und gegen 9 echte Dateien verifiziert (vorher nur Selbsttest)
- **Block&Walk-Gitterauflösung:** ist IMMER quadratisch (`max(QuadsBreite,QuadsHöhe) × 2`), nicht
  pro Achse unabhängig skaliert wie in v0.3.0 angenommen — an der nicht-quadratischen Adl-Karte
  (950×475 Quads → 1900×1900 Gitter) eindeutig widerlegt und korrigiert
  Rundungs-Tie-Breaking (round-half-away-from-zero) reproduziert dies exakt; bei Bera/Eld
  verhindern Inkonsistenzen der Original-Dateien selbst (unterschiedliche Tool-Versionen über die
  Entwicklungszeit) 100%-Bytegleichheit trotz korrekter Werte, siehe docs/MAP_FORMAT.md

### Bekannte Einschränkungen (v0.4.0)
- Texture-Layer-Auflösung ist an die Heightmap gekoppelt; reale Karten nutzen eine unabhängige,
  oft niedrigere Blend-Auflösung (z. B. 512×512 unabhängig von 257×257- oder 513×513-Heightmaps)
  — Architektur-Diskrepanz, noch nicht behoben
  aus dem NIF-Format
- `.idm`-Zellzuordnung (welche der 1178 Gruppen zu welcher Rasterzelle gehört) bleibt Hypothese
- `Eld`-Kartenset nutzt ein anderes/neueres Format (leere `.ini`, keine `.HTD`, stattdessen
  `.sbi`/`.sbisss`) — nicht abgedeckt
- `main.cpp`/`Renderer.cpp`/`Camera.cpp` weiterhin nicht in dieser Umgebung kompilierbar (siehe
  v0.1.0); alle Core-Module sind dagegen kompiliert UND gegen echte Referenzdateien getestet

## [0.3.0] — Phase 3: Block&Walk-Modul + geschlossene Roundtrip-Lücken

### Hinzugefügt
- `core::WalkGrid`: rohes int16-Gitter (512×512 bei Standard-Heightmap-Auflösung, hergeleitet
  aus 256 Quads × 2 Subzellen — siehe docs/MAP_FORMAT.md für die Korrektur der ursprünglich
  falschen "64×8"-Herleitung)
- `core::WalkEditOps`: Stempel-Pinsel (harte Kante, kein Falloff — passend für diskrete
  Flag-/Bitmask-Werte) + eigener Undo/Redo-Stack
- `core::WalkGridIO`: natives `.tswalk`-Format + `ImportLegacyShbd`/`ExportLegacyShbd` —
  **verifiziert byte-für-byte identisch** zur echten `Rou.shbd`
- `legacy::BmpBlendMap`: 8-bit-Graustufen-BMP-Codec für Blend-Bitmaps (Lesen/Schreiben) —
  schließt die Texturing-Roundtrip-Lücke aus v0.2.0 (Selbsttest, da keine `.BMP`-Referenzdatei
  verfügbar war)
- `legacy::LegacyTextureSetIO`: bündelt `.ini` + alle Blend-BMPs zu einem kompletten
  Texturing-Set-Import/-Export (inkl. Rekonstruktion der Legacy-Verzeichnisstruktur)
- GUI: 2D-Editor jetzt mit drei Modi (Heightmap / Textur malen / Block&Walk), Werkzeuge-Panel
  entsprechend erweitert, Menü um Block&Walk-Laden/Speichern/Legacy-Import/-Export ergänzt
- 11 neue Tests in `tests/test_walk_grid.cpp`, 15 neue in `tests/test_legacy_texture_roundtrip.cpp`
  (Gesamt: 56 Checks über vier Testdateien, alle grün)

### Korrigiert
- `ExportLegacyHtd` ergänzt (v0.2.0 hatte nur Import — siehe v0.2.0-Eintrag)
- Falsche Auflösungsherleitung für das Block&Walk-Gitter korrigiert, bevor sie in der GUI
  verbaut wurde (256×2 statt 64×8 — siehe docs/MAP_FORMAT.md)

### Bekannte Einschränkungen (v0.3.0)
- `.idm` (Instanz-/Index-Tabelle) und `.shmd` (Objekt-Placement-Liste) sowie `.aid`
  (Zonen-Metadaten) noch nicht implementiert — nächste Phase
- Bit-Semantik der `Rou.shbd`-Werte bleibt Hypothese (zusammenhängende Bitmasken erkannt, aber
  keine Tool-Doku zur Bestätigung verfügbar) — Editor arbeitet daher mit Rohwerten statt einer
  vermuteten Bedeutung
- `main.cpp`/`Renderer.cpp`/`Camera.cpp` weiterhin nicht in dieser Umgebung kompilierbar (siehe
  v0.1.0) — alle Core-Module (`Heightmap`, `TextureLayer*`, `WalkGrid*`, `legacy::*`) sind
  dagegen kompiliert UND gegen echte Referenzdateien getestet

## [0.2.0] — Phase 2: Texturing-Modul + Legacy-Export

### Hinzugefügt
- `core::BlendMap` / `core::TextureLayerStack`: Gewichtsgitter pro Layer, gemeinsame Gitter-
  auflösung, Basis-Layer wird automatisch voll belegt (Summe aller Layer-Gewichte = 1.0)
- `core::TexturePaintOps`: `PaintLayerWeight` mit Cross-Layer-Normalisierung (Erhöhen eines
  Layers senkt die übrigen proportional zu ihrem Anteil ab) + eigener Undo/Redo-Stack
- `core::TextureLayerIO`: natives `.tstex`-Format (Layer-Metadaten + quantisierte Gewichte)
- `core::legacy::LegacyMapIni`: gemeinsamer Parser für das `.ini`-Format (Heightmap-Metadaten
  UND Layer-Definitionen), getestet gegen den echten Inhalt von `Rou.ini`
- `ExportLegacyHtd`: Heightmap zurück ins Legacy-`.HTD`-Layout schreiben — **verifiziert
  byte-für-byte identisch** zur hochgeladenen Original-`Rou.HTD`
- `legacy::SerializeLegacyMapIni`: `.ini` zurückschreiben — verifiziert wertgleich nach
  Parse→Serialize→Reparse
- 12 neue Tests in `tests/test_texture_layers.cpp` (Gesamt: 30 Checks über beide Testdateien,
  alle grün), inkl. `tests/fixtures/Rou.ini` mit dem echten Original-Inhalt

### Bekannte Einschränkungen (v0.2.0)
- Texturing hat noch **kein GUI-Panel** in `main.cpp` (nur Heightmap ist bisher an die App
  angebunden) — folgt zusammen mit Block&Walk in einem gemeinsamen UI-Update
- Blend-**Bitmaps** (`.BMP`) werden nur als Pfad-Metadatum erfasst, kein Pixel-Import/-Export
  (keine `.BMP`-Datei in den Referenzdaten vorhanden) — siehe `docs/MAP_FORMAT.md`, Abschnitt
  "Roundtrip-Status"
- `.ini`-Export reproduziert die Nutzdaten vollständig, aber nicht die Original-Formatierung
  (Kommentare, exaktes Tab-Layout)

## [0.1.0] — Phase 1: Heightmap-Modul

### Hinzugefügt
- `core::Heightmap`: GUI-freier Datencontainer (Vertex-Grid, bilineares `SampleWorld`, `MinMax`)
- `core::HeightmapIO`: natives `.tshm`-Format (selbstbeschreibend, versioniert) + Importer für
  das reverse-engineerte Legacy-Format (`Rou.HTD`/`Rou.HTDG`), Details siehe `docs/MAP_FORMAT.md`
- `core::EditOps`: Pinsel-Operationen (Anheben/Absenken/Glätten/Einebnen) mit Smoothstep-Falloff
  + `UndoStack` (Diff-Patches, nicht komplette Gitter-Snapshots)
- Standalone-App (`map_editor`): GLFW + Dear ImGui (Docking) + OpenGL 3.3
  - 2D-Panel: Graustufen-Höhenbild, Pinsel direkt per Maus auf dem Bild
  - 3D-Panel: schattiertes Mesh mit Höhen-Farbverlauf, Orbit-Kamera, Wireframe-Toggle
  - Menü: Neu / Laden / Speichern (`.tshm`), Legacy-Import mit manueller Dimensionseingabe
- `tests/test_heightmap_core.cpp`: GUI-freier Test, baubar direkt mit `g++` (kein CMake/GL
  nötig) — verifiziert Kernlogik UND den Legacy-Importer gegen echte `Rou.HTD`/`Rou.HTDG`

### Bekannte Einschränkungen (v0.1.0)
- Nur Heightmap-Modul; Texturing / Block&Walk / Objekt-Placement folgen als eigene Module
- Ein Pinsel-"Stempel" pro Frame erzeugt ein eigenes Undo-Patch (kein Zusammenfassen ganzer
  Striche) — funktional korrekt, aber mehr Undo-Schritte als nötig bei langem Strich
- `Smooth`-Modus mittelt innerhalb eines einzelnen Anwendungsschritts direkt auf dem Live-Gitter
  (keine getrennte Lese-/Schreib-Kopie) — bei sehr großem Radius/Strength minimal abweichend von
  einem "reinen" Weichzeichner, in der Praxis für Terrain-Editing unauffällig
- `main.cpp` / `Renderer.cpp` / `Camera.cpp` sind **nicht** in dieser Umgebung kompiliert worden
  (keine GLFW/ImGui/glad-Libs, keine Netzwerkverbindung zum Nachinstallieren verfügbar) —
  Core-Modul (`Heightmap`/`HeightmapIO`/`EditOps`) inkl. Legacy-Import wurde dagegen kompiliert
  UND gegen die echten `Rou.HTD`/`Rou.HTDG`-Dateien getestet (14/14 Checks grün)

## 2026-09-16 – SHN Editor + CI Build Workflow

- SHN-Core-Parser/Writer für verschlüsselte und raw `.shn` Dateien ergänzt.
- Fiesta Crypto-Header und Verschlüsselungsalgorithmus beim Speichern erhalten.
- Unterstützung für bekannte SHN-Spaltentypen inklusive Typ 26 und Typ 29.
- Unbekannte Typen werden verlustarm als Rohbytes geladen/gespeichert.
- SHN Editor GUI mit Single-/Multi-/XP-/Buy&Sell-/Quest-Reitern begonnen.
- Zellbearbeitung per Doppelklick, Suche und mehrere gleichzeitig geöffnete SHN-Dateien ergänzt.
- GitHub Actions Build-/Test-Workflow für Windows und Linux ergänzt.

## [0.44.35-render-fix-1] – Format-3-Korrektur

- Die zuvor spekulative Zuordnung von `NiPixelData`-Pixelformat `3` zu BC1/DXT1 wurde entfernt.
- Echte NIF-Befunde und die vorhandene Dokumentation kennzeichnen Format 3 als palettiert/unkomprimiert; eine BC1-Interpretation erzeugt falsche Farben und kann Daten falsch lesen.
- Der Decoder meldet Format 3 nun explizit als noch nicht dekodierbare Variante, statt eine falsche Textur auszugeben.
- Die geprüften Formate 4/5/6 bleiben unverändert.

## NIF partial-geometry fallback
- After all strict parser/resync/variant attempts fail, the final `allowPartial` pass now preserves already validated triangle geometry when a later malformed/unsupported block is encountered.
- This prevents a late NiPixelData/effect/animation/secondary-mesh parse failure from hiding an otherwise renderable object completely.
- Verified against original client NIFs: `Adl_field_hole03.nif` now returns 22 parts / 542 triangles; `adl_barrier_mini_town.nif` now returns 56 parts / 13,901 triangles instead of failing the whole model.
- Strict parsing remains unchanged; the fallback is only the final recovery stage.

## NIF rendering follow-up — NiAlphaProperty semantics
- Preserve NiAlphaProperty source/destination blend selectors instead of forcing one blend mode.
- Map Gamebryo blend factors to OpenGL per submesh.
- Preserve the NiAlphaProperty alpha-test function and evaluate NEVER/LESS/EQUAL/LEQUAL/GREATER/NOTEQUAL/GEQUAL/ALWAYS in the fragment shader.
- Headless `mapeditor_core` build verified after parser/model changes.

## NIF rendering follow-up — UV set semantics
- Preserve all geometry UV sets instead of discarding every set after UV0.
- Preserve base TexDesc UV-set, clamp-mode and filter-mode metadata from NiTexturingProperty.
- Select the UV set requested by the base texture descriptor for the current single-texture renderer.
- Headless `mapeditor_core` build verified after the UV/TexturingProperty changes.

## NIF rendering follow-up — NiStencilProperty / FaceDrawMode (v6)
- Parse NiStencilProperty in its semantic field order instead of byte-count-only skipping.
- Resolve stencil properties through each geometry node's property references.
- Preserve FaceDrawMode per NifMeshPart and apply front/back/no culling per submesh.
- Unknown FaceDrawMode values remain double-sided to avoid disappearing geometry.
- Headless mapeditor_core build verified after parser/model changes.

## NIF rendering follow-up — multi-texture/material fidelity (v10)
- Preserve and resolve the ten classic `NiTexturingProperty` slots: Base, Dark, Detail, Gloss, Glow, Bump and Decal 0..3.
- Preserve per-slot UV-set, clamp/filter state and optional texture transforms; the renderer exposes up to eight geometry UV sets simultaneously.
- Correct the Bump Map payload length: luma scale + luma offset + Matrix22 are 24 bytes once, not twice, allowing following decal/shader descriptors to stay aligned.
- Render Dark/Detail/Gloss/Glow/Decal semantics and use the Bump Map as a height-gradient normal perturbation with the NIF luma parameters/matrix.
- Use NiMaterialProperty ambient/diffuse/specular/emissive/glossiness in the shader and honor NiSpecularProperty as the specular enable gate.
- Add Windows WIC loading for JPG/JPEG/PNG/BMP external NIF textures (in addition to the existing DDS/TGA path); this covers real Fiesta bump maps such as `water-normal.jpg` without another third-party image decoder.
- Real-NIF verification: `TevaDn02/BossRoom.nif` resolves Detail on UV1 while Base/Gloss use UV0; `RouCos01/northCoastwater.nif` resolves Glow on UV2 and Decal0 on UV1; `SD_Vale00/Water.nif` resolves Base+Dark+Detail+Gloss+Glow+Bump including `water-normal.jpg`.
- Headless core build passes; renderer C++ syntax path passes against the OpenGL stub. GUI/OpenGL runtime rendering remains unverified in this environment.
