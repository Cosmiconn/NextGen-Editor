# Map Editor v0.44.7 – Fixes

## 1. Vertikal gespiegelte Terrain-Diffuse-Texturen

Die Korrektur liegt jetzt im gemeinsamen DDS-Decoder und nicht mehr im Terrain-Shader.
DDS-Top-Mip-Zeilen werden vor dem OpenGL-Upload vertikal ausgerichtet. Blend/Block/Walk
bleiben auf ihrer bisherigen Kartenkoordinatenbasis.

## 2. NIF-Objekte ohne Texturen

`NiSourceTexture` wird jetzt als vollständige Quelle ausgewertet:

- `Use External = 1`: bisheriger externer DDS-Pfad.
- `Use External = 0`: `Pixel Data`-Referenz wird verfolgt.
- `NiPixelData` wird vollständig gelesen, der erste/größte Mipmap-Level wird dekodiert.
- DXT1/DXT3/DXT5 werden unterstützt; bei den Fiesta-NIFs wurde zusätzlich die vorkommende
  8-Byte-DXT1-kompatible Speicherung trotz DXT5-Kennung erkannt.
- Das Ergebnis wird als RGBA8 an den NIF-Renderer übergeben.
- Der im NIF vorhandene `.dds`-Dateiname bleibt als Metadatum erhalten und wird bei
  eingebetteten Texturen nicht mehr als externe Datei vorausgesetzt.

## Verifikation

- `Rou_M_Shop01.nif`: eingebettete Textur erfolgreich dekodiert.
- Testkorpus: 83 NIFs geprüft, 70 erfolgreich geladen; 65 enthalten erfolgreich zugeordnete
  eingebettete Diffuse-Texturen.
- Keine `NiPixelData`-Decodefehler im Massentest.

Die vollständige GUI/OpenGL-Anwendung konnte in der aktuellen Linux-Umgebung nicht gebaut
werden, weil die GLFW-CMake-Paketdateien bzw. GLAD-Header dort fehlen. Der Core-Parser und
DDS-Decoder wurden separat mit GCC kompiliert und getestet.
