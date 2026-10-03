# 05 – Modul-Blueprints

## Topbar
Kompakte Markenleiste, Menüs, Projekt/Map-Kontext, Settings. NG-Small-Mark links.

## Primary Toolbar
Transform-Gruppe dominant; World/Gameplay klar getrennt; identische Button-Geometrie.

## 3D Viewport
Hero-Fläche, kompakte Overlaybar, FPS/Objektstatus unten rechts, keine permanente Hilfe.

## 2D / Minimap
Gleiche Header/Overlaybuttons, klare Layer-/Walk-Legende.

## Scene Outliner
Eye/Lock/Semantic Icon/Name/Badge. Gruppen + Suche ohne visuelle Unruhe.

## Inspector
Transform zuerst, dann Modell/Daten, Anzeige, Spezial. Klickbare Referenzen statt Textwüste.

## Asset Browser
Folder Tree links, Grid/List rechts, Suche oben, große Thumbnail-Cards, klarer Drag-State.

## Terrain
Brush Preview + Radius/Strength/Hardness kompakt. Raise/Lower/Smooth/Flatten als Toolstrip.

## Layers
42–52 px Thumbnail, Eye/Lock, Name, UV/Blend-Meta, DnD-Reorder.

## Block & Walk
Rot/Grün-Legende, Brush/Rectangle, sichtbares Undo/Redo. Footprint→Walk verwendet die echte NIF-Grundfläche (konvexe Hull, Bounding-Fallback) und folgt verbindlich **Preview → Apply/Cancel**: sichtbare Kategorien werden im 2D-View rot/grün gefüllt, Apply ist genau ein Undo-Schritt.

## SHN
Daten-IDE statt Roh-Tabelle: Dokumentkopf, Dirty, Filter, Save-Bar. Referenz-/Error-Hinweise sind evidenzbasiert: aktuell Item/Mob/ActiveSkill-Familien per verifizierter ID-Menge; Grün=vorhanden, Rot=fehlend, Gelb=mehrdeutig, eindeutige Treffer als Cross-Link.

## Quest
General / Requirements / Objectives / Rewards / Dialogs / Scripts. Referenzen als Links. Form/Flow-Umschaltung im Detailbereich: Flow bleibt read-only und verwendet ausschließlich die belegten `needPred/predecessor`-Kanten, zeigt Vorgänger-Kette, direkte Folgequests sowie Zyklus-/Missing-/Duplicate-ID-Zustände. Scripttext wird nicht heuristisch als Graph interpretiert.

## Skill
Serie links, Stufe/Detail rechts. Animation/VFX nutzen datenbelegte Such-Picker über alle gleichartigen ActiveSkillView-Felder; rechts im Picker steht eine Referenz-Vorschau mit Häufigkeit, konkreten Skills und Quellspalten sowie direkter Navigation. Das ist bewusst noch kein KF/NIF-Playback und keine Behauptung physischer Asset-Gültigkeit.

## KFM
Assetliste + Transport + echte Text-Key-Timeline + Trackliste. Der Preview-Viewport zeigt die reale KFM-NIF-Hierarchie als animiertes Skelett und deformiert über bewahrte NiSkin-Weights/Bind-Matrizen die echten NIF-Dreiecke CPU-seitig synchron zur Timeline. Das Mesh wird als budgetiertes Wireframe hinter dem Skeleton dargestellt; Drag/Zoom/Reset gehören zum Viewport. Nicht verifizierte B-Splines/TBC/Quadratic sowie mehrdeutige Node-/Track-Namen bleiben sichtbar in Bind-Pose. Ein material-/texturierter Character-Preview ist separate Politur, kein Ersatz für die verifizierte Deformation.

## AI / Interface / Drops
Gleiche Shell, Komponenten und Zustandslogik; keine optischen Mikrowelten. Haupt-Header laufen über denselben semantischen Icon-/Titel-/Kontext-Component wie Map/KFM/Quest/Skill; Dirty/Read-only-Zustände bleiben kontextuell und werden nicht als abweichendes Mini-Theme dargestellt.
