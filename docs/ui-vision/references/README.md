# Raster-Referenzen – Design Review 25.09.2026

Diese Datei ist das **Manifest** der vom Nutzer gelieferten UI-Referenzen. Die Rasterdateien selbst sind noch nicht vollständig im Git-Branch archiviert; das Manifest darf deshalb nicht mit „bereits eingecheckt“ verwechselt werden.

## Verbindliche visuelle Rollen

Die Referenzen definieren gemeinsam das Zielbild:

1. **NG-Monogramm / Branding**
   - metallisch, tiefblau/cyan, starke Fasen, helle Kanten
   - verbindliche Form-/Materialreferenz für App-/EXE-/Window-/Taskbar-Branding

2. **Icon-System / Navigation / Design-Prinzipien**
   - klare Silhouetten
   - konsistente Größen und optische Box
   - Blue/Cyan = aktiv/fokussiert

3. **Editor-Layout mit 3D + Outliner + Inspector + Asset Browser**
   - große zentrale Arbeitsfläche
   - kompakte Toolbar
   - klare Panelhierarchie

4. **Premium-Shell / Hero-Viewport**
   - World-/Layer-Struktur links
   - großer 3D-Viewport
   - Objektbibliothek, Minimap und Tool-Docks integriert

5. **Integrierte Gesamtvision**
   - App-Shell, Kontextpanel, Suche/Filter, Projekt-/Exportbereich und Modul-Icons in einer Sprache

## Aktuell bekannte Originalnamen aus den Design-Reviews

Frühere Review-Runde:

- `F4B2B5A0-473F-48E7-BEB0-461E03093175.jpeg` – NG-Monogramm / Branding
- `85A3C523-0443-43EF-9CBE-127055BF6B0A.jpeg` – Icon-/Design-Prinzipien
- `403976A3-1ABB-4EA6-A41B-FED0CA39A7AB.jpeg` – Editor-Layout
- `34D1687A-14C0-4112-8FA3-AFFE525069C2.jpeg` – Premium-Shell
- `9AE91D85-8415-41F4-AE44-29C1493A3F85.jpeg` – Gesamtvision

Aktuelle Review-Runde im neuen Chat:

- `76E156A9-8A81-4A2F-85CE-027C4E612288.jpeg` – UI/UX-Designspezifikation mit NG-Brand, Toolbar-/Navigation-/Panel-Icons und Referenzlayout
- `9AE91D85-8415-41F4-AE44-29C1493A3F85(1).jpeg` – integrierte Gesamtvision
- `34D1687A-14C0-4112-8FA3-AFFE525069C2(1).jpeg` – großer Map-Editor / Hero-Viewport
- `IMG_9935.jpeg` – Ist-Zustand des aktuellen NextGen-Editors als visuelle Gap-Referenz

## Priorität bei Konflikten

1. freigegebenes metallisches NG-Monogramm
2. integrierte Gesamtarchitektur / Premium-Shell
3. Viewport-/Dock-Verhältnis
4. Toolbar-/Inspector-Dichte
5. Icon-Prinzipien und Zustände

## Archivierungsstatus

Eine Repository-Prüfung am Branch `ui-upgrade` hat bestätigt, dass die oben aufgeführten JPEG-Dateien **noch nicht als Binärdateien unter `docs/ui-vision/reference/` eingecheckt sind**.

Darum gilt:

- dieses Manifest hält die Designabsicht fest;
- die Binärreferenzen werden separat archiviert, sobald die bereitgestellten Upload-Bytes im Arbeitsruntime verfügbar sind;
- bis dahin dürfen keine generierten Platzhalterdateien an ihrer Stelle eingecheckt werden;
- Mockup-SVGs/Wireframes sind nur strukturelle Hilfen, nicht Ersatz für die Rasterreferenzen.

## Abnahmeregel

Wenn Implementierung, Markdown-Spezifikation und Rasterreferenz voneinander abweichen, gewinnt die vom Nutzer freigegebene Rasterreferenz. Funktional nicht vorhandene Elemente werden trotzdem nicht als funktional dargestellt.
