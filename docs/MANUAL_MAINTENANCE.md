# Handbuch, Tooltips und Spalten-Referenz pflegen

Alle Texte sind zweisprachig (Deutsch/Englisch) und liegen als Python-Datenquellen in `tools/manual/`:

| Datei | Inhalt |
|---|---|
| `sections.py` | Kapitel und Handbuch-Abschnitte (`S(id, kapitel, titelDE, titelEN, textDE, textEN, stichworte)`). Textformat: `# ` = Zwischenüberschrift, `- ` = Aufzählung, sonst Absatz. |
| `tooltips.py` | Tooltips je Bedienelement: Schlüssel = Beschriftung (mit `##Suffix` bevorzugt), `"<Art>:<Beschriftung>"` bei gleicher Beschriftung in verschiedenen Widget-Arten (z.B. `SmallButton:X` / `InputInt:X`), `"T:<schluessel>"` für Beschriftungen aus der Übersetzungstabelle (`T("...")`). |
| `columns.py` | Spalten-Referenz (Tabelle, Spalte, sicher?, DE, EN). `sicher=False` erscheint als "(vermutet)". Die Skill-Tabellen werden automatisch aus den `skilled::`-Feldlisten in `src/app/main.cpp` übernommen. |

**Nach jeder Änderung:** `python3 tools/manual/gen.py` (schreibt `src/core/ManualData.cpp`, generiert - nicht von Hand ändern), dann bauen.

**Neue Bedienelemente:** in `main.cpp` immer `UI::Button/Checkbox/SliderFloat/InputInt/InputText/Combo/...` statt `ImGui::...` verwenden - die `UI::`-Hülle zeigt den Tooltip aus `tooltips.py` automatisch. Fehlt der Eintrag, schlägt `test_manual <pfad>/src/app/main.cpp` fehl (Abdeckungsprüfung: jedes Bedienelement mit Text-Literal muss einen Tooltip in DE und EN haben).

**Grundsatz:** nur beschreiben, was aus Daten oder Verhalten belegt ist; Abgeleitetes als `(vermutet)` kennzeichnen.
