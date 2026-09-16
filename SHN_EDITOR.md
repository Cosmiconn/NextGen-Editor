# SHN Editor – NextGen-Editor

Der SHN Editor ist als eigener Bereich im NextGen-Editor integriert.

## Client / Server Trennung

Der Editor führt SHN-Dateien ausdrücklich nach Quelle getrennt und immer in dieser Reihenfolge:

1. **CLIENT** – zuerst
2. **SERVER** – danach

Für beide Seiten kann ein SHN-Wurzelordner rekursiv eingelesen werden. Einzelne Dateien können zusätzlich geöffnet werden; die Herkunft wird aus dem gewählten Ordner bzw. dem Pfad übernommen.

Diese Trennung gilt identisch für **Single SHN Editor** und **Multi SHN Editor**.

## Unterstützte Funktionen

- verschlüsselte Fiesta-`.shn` Dateien öffnen
- unverschlüsselte/raw SHN Dateien öffnen
- 32-Byte Crypto-Header erhalten
- Tabellenkopf, Zeilenanzahl, Spaltennamen, Spaltentypen und Spaltenlängen lesen
- Tabellenansicht mit horizontalem/vertikalem Scrolling
- Suche über Spaltennamen und/oder Zellwerte
- Doppelklick auf eine Zelle → Wert bearbeiten
- Änderungen wieder in das SHN-Format schreiben
- mehrere SHN Dateien gleichzeitig geöffnet halten
- unbekannte Spaltentypen als Rohbytes behandeln
- Typ 29 als `UInt32:UInt32`
- Typ 26 als variable Null-terminierte Zeichenkette

## Multi-SHN Aufgabenprofile

Der Multi-SHN Editor bietet Kandidatenprofile für:

- Neues Item
- Neuer NPC
- Neuer Mob
- Neuer Skill
- Shop / Preis
- Neue Quest
- XP / Rate

Die Kandidaten werden getrennt nach CLIENT und SERVER dargestellt und gelb markiert. Die Markierung bedeutet **prüfen/ggf. ändern** und wird nicht als bewiesene technische Abhängigkeit ausgegeben. Die endgültige Abhängigkeitsmatrix wird aus den realen SHN-/Datenbeziehungen des jeweiligen Client-/Serverstands verifiziert.

## Noch folgende Komfortfunktionen

- echte Save-As-Dateiauswahl
- Cell-/Row-Undo/Redo
- CSV/Clipboard-Import/Export
- spezielle XP-/Preis-/Quest-Assistenten
- automatische ID-Vergabe und Duplikatprüfung
