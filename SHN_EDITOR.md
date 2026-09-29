# SHN Editor – NextGen-Editor

Der SHN Editor ist jetzt als eigener Bereich im NextGen-Editor integriert.

## Unterstützte Funktionen

- verschlüsselte Fiesta-`.shn` Dateien öffnen
- unverschlüsselte/raw SHN Dateien öffnen
- 32-Byte Crypto-Header erhalten
- Tabellenkopf, Zeilenanzahl, Spaltennamen, Spaltentypen und Spaltenlängen lesen
- Tabellenansicht mit horizontalem/vertikalem Scrolling
- Suche über Spaltennamen und/oder Zellwerte
- Doppelklick auf eine Zelle → Wert bearbeiten
- Änderungen direkt wieder in das ursprüngliche SHN-Format schreiben
- mehrere SHN Dateien gleichzeitig geöffnet halten
- unbekannte Spaltentypen werden als Rohbytes angezeigt, statt die Datei beim Laden abzulehnen
- Typ 29 wird als `UInt32:UInt32` dargestellt
- Typ 26 wird als variable Null-terminierte Zeichenkette behandelt

## Bekannte Fiesta-Typen

Die Implementierung basiert auf der öffentlich dokumentierten ShineTableParser-Struktur und wurde zusätzlich gegen die mitgelieferten Client-SHN-Dateien geprüft. Bekannte Typen umfassen 1/12/16, 2, 3/11/18/27, 5, 9/24, 13/21, 20, 22, 26 und 29.

## Sicherheitsprinzip

Der Editor verändert keine Clientdatei automatisch. `Speichern` schreibt erst nach ausdrücklichem Klick auf den Speichern-Button. Vor Änderungen sollte eine Kopie des Original-Clients verwendet werden.

## Noch folgende Komfortfunktionen

- echte Save-As-Dateiauswahl
- Cell-/Row-Undo/Redo
- CSV/Clipboard-Import/Export
- spezielle XP-/Preis-/Quest-Assistenten
- automatische ID-Vergabe und Duplikatprüfung

Diese Funktionen bauen auf derselben SHN-Core-Schicht auf und müssen das Binärformat nicht erneut implementieren.

## Client / Server Trennung

Der Editor führt SHN-Dateien jetzt ausdrücklich nach Quelle getrennt:

1. **CLIENT** – zuerst
2. **SERVER** – danach

Für beide Seiten kann ein SHN-Wurzelordner rekursiv eingelesen werden. Einzelne Dateien können zusätzlich geöffnet werden; die Herkunft wird aus dem gewählten Ordner bzw. dem Pfad übernommen.

Diese Trennung gilt identisch für **Single SHN Editor** und **Multi SHN Editor**.

## Multi-SHN Aufgabenprofile

Der Multi-SHN Editor bietet Kandidatenprofile für:

- Neues Item
- Neuer NPC
- Neuer Mob
- Neuer Skill
- Shop / Preis
- Neue Quest
- XP / Rate

Die Kandidaten werden getrennt nach CLIENT und SERVER dargestellt und gelb markiert. Die Markierung bedeutet **prüfen/ggf. ändern**; sie wird nicht als bewiesene technische Abhängigkeit ausgegeben. Die endgültige Abhängigkeitsmatrix soll aus den realen SHN-/Datenbeziehungen des jeweiligen Client-/Serverstands verifiziert werden.
