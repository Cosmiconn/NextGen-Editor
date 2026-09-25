# NextGen Editor – Icon Inventory

**Status:** verbindliche Inventar-/Mapping-Datei für `ui-upgrade`  
**Geprüft am:** 2026-09-25

## Verbindliche Quelle – Final-Paket

**`NextGen_Icons_Final.zip`** ist die alleinige verbindliche Quelle für neue Icon-Arbeiten.

Technische Prüfung des Archivs:
- `manifest.csv`: **68** logische Icons.
- Gruppen: Toolbar **18**, Navigation **15**, Panels **12**, Extra/Module/Branding **9**, Additional UI **14**.
- `svg/`: **68/68** echte pfadbasierte SVGs; **0/68** enthalten `<image>`, `data:image` oder Base64-Rasterdaten.
- `icons_png/16|24|32|48|64|128`: **68 Icons je Größe** (408 Small-Size-Raster insgesamt).
- `png_256/` und `png_1024/`: hochwertige transparente Rasterreferenzen.
- `svg_embedded_png_256|1024/` sowie die Button-Tile-Gegenstücke: **272** SVG-Container mit eingebetteten PNGs; diese sind bewusst **keine** Vector-Master.
- SHA-256 des geprüften Final-Pakets: `4057d096eb71cd9ef84bdcf5741afa66f85148ae5ffa55e82c87ff30e50e1b77`.

Die älteren Pakete sind nur noch Provenienz. Die ursprünglichen 54 SVG-/Small-Size-Assets sind im Final-Paket byte-identisch enthalten; zusätzlich kommen 14 fehlende UI-Icons hinzu.

## Source-of-truth-Regeln

- Kein Ersatzicon erfinden, solange für die Funktion ein freigegebenes Paketicon existiert.
- Keine Mischung aus freigegebenem Set und generischen Lucide-/Emoji-/Font-Icons im finalen UI.
- Das metallisch blau/cyan leuchtende **NG** ist die verbindliche Marke.
- UI-Code referenziert stabile semantische IDs, niemals Paketdateinamen direkt.
- Bei 16–32 px haben die vorbereiteten Small-Size-PNGs Vorrang vor Live-SVG-Rasterisierung.
- Die echten SVGs bleiben als Master-/Archivquelle im Repository erhalten.
- Fehlende Icons werden erst nach Abgleich gegen alle 68 Final-Paket-Einträge als echter Bedarf dokumentiert.

## Paketabdeckung

- Toolbar: **18**
- Navigation: **15**
- Panels: **12**
- Extra/Module/Branding: **9**
- Additional UI: **14**
- Summe: **68**

## Vollständiges Soll-/Ist-Mapping

| Gruppe | Nr | Paketname | Logische ID | Funktion | Editor-Verwendung | Vector-Master | Runtime-Raster | Status |
|---|---:|---|---|---|---|---|---|---|
| toolbar | 01 | neu | `file.new` | Neu / New | App shell / Datei | `svg/01_toolbar_icons/01_neu.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/01_neu.png` | verified / mapping frozen |
| toolbar | 02 | oeffnen | `file.open` | Öffnen / Open | App shell / Datei | `svg/01_toolbar_icons/02_oeffnen.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/02_oeffnen.png` | verified / mapping frozen |
| toolbar | 03 | speichern | `file.save` | Speichern / Save | App shell / Datei | `svg/01_toolbar_icons/03_speichern.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/03_speichern.png` | verified / mapping frozen |
| toolbar | 04 | rueckgaengig | `history.undo` | Rückgängig / Undo | App shell / Historie | `svg/01_toolbar_icons/04_rueckgaengig.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/04_rueckgaengig.png` | verified / mapping frozen |
| toolbar | 05 | wiederholen | `history.redo` | Wiederholen / Redo | App shell / Historie | `svg/01_toolbar_icons/05_wiederholen.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/05_wiederholen.png` | verified / mapping frozen |
| toolbar | 06 | auswaehlen | `transform.select` | Auswählen / Select | Map toolbar / Transform | `svg/01_toolbar_icons/06_auswaehlen.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/06_auswaehlen.png` | verified / mapping frozen |
| toolbar | 07 | verschieben | `transform.move` | Verschieben / Move | Map toolbar / Transform | `svg/01_toolbar_icons/07_verschieben.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/07_verschieben.png` | verified / mapping frozen |
| toolbar | 08 | rotieren | `transform.rotate` | Rotieren / Rotate | Map toolbar / Transform | `svg/01_toolbar_icons/08_rotieren.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/08_rotieren.png` | verified / mapping frozen |
| toolbar | 09 | skalieren | `transform.scale` | Skalieren / Scale | Map toolbar / Transform | `svg/01_toolbar_icons/09_skalieren.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/09_skalieren.png` | verified / mapping frozen |
| toolbar | 10 | pinsel | `tool.brush` | Pinsel / Brush | Terrain / Paint / Walk | `svg/01_toolbar_icons/10_pinsel.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/10_pinsel.png` | verified / mapping frozen |
| toolbar | 11 | terrain | `world.terrain` | Terrain | Map toolbar / Terrain | `svg/01_toolbar_icons/11_terrain.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/11_terrain.png` | verified / mapping frozen |
| toolbar | 12 | layer | `world.layers` | Layer | Map toolbar / Layer | `svg/01_toolbar_icons/12_layer.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/12_layer.png` | verified / mapping frozen |
| toolbar | 13 | objekte | `world.objects` | Objekte / Objects | Map toolbar / Objects | `svg/01_toolbar_icons/13_objekte.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/13_objekte.png` | verified / mapping frozen |
| toolbar | 14 | block_and_walk | `gameplay.block_walk` | Block & Walk | Map toolbar / Walk & Block | `svg/01_toolbar_icons/14_block_and_walk.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/14_block_and_walk.png` | verified / mapping frozen |
| toolbar | 15 | kollision | `gameplay.collision` | Kollision / Collision | Map toolbar / Collision | `svg/01_toolbar_icons/15_kollision.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/15_kollision.png` | verified / mapping frozen |
| toolbar | 16 | pfad | `gameplay.path` | Pfad / Path | Map toolbar / Pfad/Route | `svg/01_toolbar_icons/16_pfad.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/16_pfad.png` | verified / mapping frozen |
| toolbar | 17 | licht | `world.light` | Licht / Light | Map toolbar / Lighting | `svg/01_toolbar_icons/17_licht.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/17_licht.png` | verified / mapping frozen |
| toolbar | 18 | play-test | `system.playtest` | Play/Test | App shell / Testlauf | `svg/01_toolbar_icons/18_play-test.svg` | `icons_png/{16,24,32,48,64,128}/01_toolbar_icons/18_play-test.png` | verified / mapping frozen |
| navigation | 01 | welt | `nav.world` | Welt / World | Map navigation | `svg/02_navigation_icons/01_welt.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/01_welt.png` | verified / mapping frozen |
| navigation | 02 | terrain | `nav.terrain` | Terrain | Map navigation | `svg/02_navigation_icons/02_terrain.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/02_terrain.png` | verified / mapping frozen |
| navigation | 03 | malen | `nav.paint` | Malen / Paint | Map navigation | `svg/02_navigation_icons/03_malen.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/03_malen.png` | verified / mapping frozen |
| navigation | 04 | ebenen | `nav.layers` | Ebenen / Layers | Map navigation | `svg/02_navigation_icons/04_ebenen.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/04_ebenen.png` | verified / mapping frozen |
| navigation | 05 | objekte | `nav.objects` | Objekte / Objects | Map navigation | `svg/02_navigation_icons/05_objekte.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/05_objekte.png` | verified / mapping frozen |
| navigation | 06 | wasser | `nav.water` | Wasser / Water | World navigation | `svg/02_navigation_icons/06_wasser.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/06_wasser.png` | verified / mapping frozen |
| navigation | 07 | himmel | `nav.sky` | Himmel / Sky | World navigation | `svg/02_navigation_icons/07_himmel.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/07_himmel.png` | verified / mapping frozen |
| navigation | 08 | flora | `nav.flora` | Flora | World navigation | `svg/02_navigation_icons/08_flora.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/08_flora.png` | verified / mapping frozen |
| navigation | 09 | beleuchtung | `nav.lighting` | Beleuchtung / Lighting | World navigation | `svg/02_navigation_icons/09_beleuchtung.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/09_beleuchtung.png` | verified / mapping frozen |
| navigation | 10 | rendering | `nav.rendering` | Rendering | Rendering / visibility navigation | `svg/02_navigation_icons/10_rendering.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/10_rendering.png` | verified / mapping frozen |
| navigation | 11 | npcs | `nav.npcs` | NPCs | Gameplay navigation | `svg/02_navigation_icons/11_npcs.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/11_npcs.png` | verified / mapping frozen |
| navigation | 12 | punkte | `nav.points` | Punkte / Points | Waypoints / point markers | `svg/02_navigation_icons/12_punkte.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/12_punkte.png` | verified / mapping frozen |
| navigation | 13 | spawnpunkte | `nav.spawns` | Spawnpunkte / Spawns | Spawn navigation | `svg/02_navigation_icons/13_spawnpunkte.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/13_spawnpunkte.png` | verified / mapping frozen |
| navigation | 14 | trigger | `nav.trigger` | Trigger | Trigger navigation | `svg/02_navigation_icons/14_trigger.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/14_trigger.png` | verified / mapping frozen |
| navigation | 15 | events | `nav.events` | Events | Event navigation | `svg/02_navigation_icons/15_events.svg` | `icons_png/{16,24,32,48,64,128}/02_navigation_icons/15_events.png` | verified / mapping frozen |
| panel | 01 | projekt | `panel.project` | Projekt / Project | Panel / launcher | `svg/03_panel_icons/01_projekt.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/01_projekt.png` | verified / mapping frozen |
| panel | 02 | asset_browser | `panel.asset_browser` | Asset Browser | Panel header / launcher | `svg/03_panel_icons/02_asset_browser.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/02_asset_browser.png` | verified / mapping frozen |
| panel | 03 | objektliste | `panel.outliner` | Objektliste / Outliner | Panel header | `svg/03_panel_icons/03_objektliste.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/03_objektliste.png` | verified / mapping frozen |
| panel | 04 | eigenschaften | `panel.properties` | Eigenschaften / Properties | Inspector header | `svg/03_panel_icons/04_eigenschaften.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/04_eigenschaften.png` | verified / mapping frozen |
| panel | 05 | minimap | `panel.minimap` | Minimap | Minimap panel | `svg/03_panel_icons/05_minimap.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/05_minimap.png` | verified / mapping frozen |
| panel | 06 | werkzeuge | `panel.tools` | Werkzeuge / Tools | Tools panel | `svg/03_panel_icons/06_werkzeuge.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/06_werkzeuge.png` | verified / mapping frozen |
| panel | 07 | suche | `panel.search` | Suche / Search | Search action | `svg/03_panel_icons/07_suche.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/07_suche.png` | verified / mapping frozen |
| panel | 08 | filter | `panel.filter` | Filter | Filter action | `svg/03_panel_icons/08_filter.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/08_filter.png` | verified / mapping frozen |
| panel | 09 | einstellungen | `panel.settings` | Einstellungen / Settings | Settings | `svg/03_panel_icons/09_einstellungen.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/09_einstellungen.png` | verified / mapping frozen |
| panel | 10 | hilfe | `panel.help` | Hilfe / Help | Help | `svg/03_panel_icons/10_hilfe.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/10_hilfe.png` | verified / mapping frozen |
| panel | 11 | verifizierung | `panel.validation` | Verifizierung / Validation | Validation / audit | `svg/03_panel_icons/11_verifizierung.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/11_verifizierung.png` | verified / mapping frozen |
| panel | 12 | export | `panel.export` | Export | Export | `svg/03_panel_icons/12_export.svg` | `icons_png/{16,24,32,48,64,128}/03_panel_icons/12_export.png` | verified / mapping frozen |
| extra | 01 | SHN Editor | `module.shn.single` | Single SHN | Spieldaten launcher | `svg/04_extra_icons/01_shn_editor.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/01_shn_editor.png` | verified / mapping frozen |
| extra | 02 | Multi SHN | `module.shn.multi` | Multi SHN | Spieldaten launcher | `svg/04_extra_icons/02_multi_shn.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/02_multi_shn.png` | verified / mapping frozen |
| extra | 03 | Quest Editor | `module.quest` | Quest Editor | Spieldaten launcher | `svg/04_extra_icons/03_quest_editor.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/03_quest_editor.png` | verified / mapping frozen |
| extra | 04 | Skill Editor | `module.skill` | Skill Editor | Spieldaten launcher | `svg/04_extra_icons/04_skill_editor.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/04_skill_editor.png` | verified / mapping frozen |
| extra | 05 | Interface Editor | `module.interface` | Interface Editor | Spieldaten launcher | `svg/04_extra_icons/05_interface_editor.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/05_interface_editor.png` | verified / mapping frozen |
| extra | 06 | Droptable | `module.droptable` | Drop Table | Spieldaten launcher | `svg/04_extra_icons/06_droptable.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/06_droptable.png` | verified / mapping frozen |
| extra | 07 | Custom NPC | `module.custom_npc` | Custom NPC | Wizard launcher | `svg/04_extra_icons/07_custom_npc.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/07_custom_npc.png` | verified / mapping frozen |
| extra | 08 | Custom Mob | `module.custom_mob` | Custom Mob | Wizard launcher | `svg/04_extra_icons/08_custom_mob.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/08_custom_mob.png` | verified / mapping frozen |
| extra | 09 | NG Icon | `brand.ng` | NG Branding | EXE / Window / Taskbar / branding | `svg/04_extra_icons/09_ng_icon.svg` | `icons_png/{16,24,32,48,64,128}/04_extra_icons/09_ng_icon.png` | verified / mapping frozen |
| additional_ui | 01 | 2D | `view.2d` | 2D View | 2D viewport / tabs | `svg/05_additional_ui_icons/01_2d.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/01_2d.png` | verified / mapping frozen |
| additional_ui | 02 | 3D | `view.3d` | 3D View | 3D viewport / tabs | `svg/05_additional_ui_icons/02_3d.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/02_3d.png` | verified / mapping frozen |
| additional_ui | 03 | KFM | `module.kfm` | KFM / Animation | Animation workspace launcher | `svg/05_additional_ui_icons/03_kfm.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/03_kfm.png` | verified / mapping frozen |
| additional_ui | 04 | AI | `module.ai` | AI / Lua | AI workspace launcher | `svg/05_additional_ui_icons/04_ai.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/04_ai.png` | verified / mapping frozen |
| additional_ui | 05 | XP | `module.xp` | XP | XP editor launcher | `svg/05_additional_ui_icons/05_xp.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/05_xp.png` | verified / mapping frozen |
| additional_ui | 06 | Preise | `module.prices` | Preise / Prices | Buy & Sell editor launcher | `svg/05_additional_ui_icons/06_preise.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/06_preise.png` | verified / mapping frozen |
| additional_ui | 07 | Visibility Eye | `state.visibility` | Visibility | Outliner / visibility controls | `svg/05_additional_ui_icons/07_visibility_eye.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/07_visibility_eye.png` | verified / mapping frozen |
| additional_ui | 08 | Lock | `state.lock` | Lock | Locked state / action | `svg/05_additional_ui_icons/08_lock.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/08_lock.png` | verified / mapping frozen |
| additional_ui | 09 | Unlock | `state.unlock` | Unlock | Unlocked state / action | `svg/05_additional_ui_icons/09_unlock.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/09_unlock.png` | verified / mapping frozen |
| additional_ui | 10 | Copy | `edit.copy` | Copy | Copy action | `svg/05_additional_ui_icons/10_copy.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/10_copy.png` | verified / mapping frozen |
| additional_ui | 11 | Duplicate | `edit.duplicate` | Duplicate | Duplicate action | `svg/05_additional_ui_icons/11_duplicate.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/11_duplicate.png` | verified / mapping frozen |
| additional_ui | 12 | Delete | `edit.delete` | Delete | Destructive delete action | `svg/05_additional_ui_icons/12_delete.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/12_delete.png` | verified / mapping frozen |
| additional_ui | 13 | Command Palette | `system.command_palette` | Command Palette | Global command palette | `svg/05_additional_ui_icons/13_command_palette.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/13_command_palette.png` | verified / mapping frozen |
| additional_ui | 14 | Recent Projects | `system.recent_projects` | Recent Projects | Project/map recent lists | `svg/05_additional_ui_icons/14_recent_projects.svg` | `icons_png/{16,24,32,48,64,128}/05_additional_ui_icons/14_recent_projects.png` | verified / mapping frozen |


## Bewusste semantische Überschneidungen

- `terrain` und `objekte` existieren sowohl als Toolbar- als auch Navigation-Variante. Das sind **Kontextvarianten**, keine versehentlichen Dubletten.
- `licht` (Toolbar) und `beleuchtung` (Navigation) werden getrennt für direkte Aktion bzw. Bereichsnavigation eingesetzt.
- `punkte` ist für allgemeine Punkte/Waypoints/Marker vorgesehen; `spawnpunkte` ausschließlich für Spawn-Systeme.
- `verifizierung` bedeutet Validierung/Audit und darf nicht als generisches „OK/Apply“-Symbol missbraucht werden.

## Nach Final-Inventar tatsächlich noch fehlende dedizierte Icons

Die 14 neuen Final-Paket-Icons schließen 2D, 3D, KFM, AI, XP, Preise, Visibility, Lock/Unlock, Copy, Duplicate, Delete, Command Palette und Recent Projects.

Weiterhin **nicht** als eigenes Final-Paket-Icon vorhanden sind:
- Focus Selection
- Drop to Ground
- Local / World
- Snap
- Portal als eigener Hauptmodus
- generisches Mob-Hauptsymbol außerhalb von Spawn/Custom Mob

Bis zu einer expliziten Ergänzung bleiben hierfür die bestehenden funktionalen DrawList-Fallbacks. Es wird kein unpassendes Paketicon umgedeutet.

## Runtime-Strategie

- Haupttoolbar: 24 px Small-Size-PNG.
- Panel-/Inline-Icons: 16 px.
- Modul-Launcher: 32 px.
- NG Branding: 16/24/32/48/64/128 und für Windows zusätzlich 256 px aus dem freigegebenen Branding-Master.
- SVG-Master werden nicht pro Frame gerastert; sie dienen als skalierbare Source-of-Truth und für zukünftige Exporte.
- Rasterassets werden einmalig geladen/gecached und als OpenGL-Texturen in ImGui verwendet.

## Integrationsstatus

- [x] Final-ZIP technisch geprüft.
- [x] 68 logische Icons inventarisiert.
- [x] 68 echte SVG-Vector-Master verifiziert.
- [x] 408 Small-Size-PNGs (68 × 6 Größen) verifiziert.
- [x] 272 embedded-PNG-SVGs korrekt als Rastercontainer klassifiziert.
- [x] semantische IDs inklusive 14 Additional-UI-Icons eingefroren.
- [x] Runtime-Loader/Cache für semantische IDs vorhanden.
- [x] Primary Map Toolbar nutzt bereits Paketassets, wenn das jeweilige Runtime-PNG im Repo liegt.
- [ ] alle 68 SVG-Master nach `assets/ui/icons/svg-master/` übernehmen.
- [ ] alle Runtime-Raster nach `assets/ui/icons/png/` übernehmen (die aktuell verwendeten 16/24/32-px-Assets sind bereits eingecheckt; Rest folgt bedarfsweise/über Importer).
- [x] NG Branding in der App-Topbar über das finale 32-px-Paketasset aktiviert.
- [x] App-Shell-Aktionen Neu / Öffnen / Speichern nutzen die finalen Paketicons in einer kompakten Icon+Text-Darstellung.
- [ ] echte NG-SVG-Quelle nach `assets/ui/branding/` übernehmen und Windows-ICO final aus dem Final-Paket regenerieren.
- [x] 14 neue Additional-UI-Icons an reale Controls angebunden (2D/3D, KFM, AI, XP, Preise, Eye, Lock/Unlock, Copy, Duplicate, Delete, Command Palette, Recent Projects).
- [x] zentrale Map-Panel-Header (Outliner, Asset Browser, Properties, Visibility, Layer/Tools) sowie Settings/Help auf Paketicons migriert.
- [ ] übrige Panel Header und Modul-Launcher vollständig migrieren.
- [ ] Legacy-DrawList-Symbole nur für die sechs tatsächlich verbleibenden Paketlücken beibehalten.

## Runtime-QA-Hinweis

Windows WIC liefert PNG-Daten für den gemeinsamen Renderer in OpenGL-Bottom-Up-Konvention. Die UI-Icon-Renderer drehen daher die ImGui-UVs explizit (`uv0=(0,1)`, `uv1=(1,0)`), damit die Paketicons im UI nicht vertikal gespiegelt erscheinen. Der Windows-Artefakt-Job paketiert `assets/ui` jetzt zusammen mit `Editor.exe`.
