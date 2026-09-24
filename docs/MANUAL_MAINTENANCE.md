# Handbuch pflegen

Seit v13 werden die zweisprachigen Handbuch-, Tooltip- und Spaltenbeschreibungen direkt in
`src/core/ManualData.cpp` gepflegt. Die früheren Python-Generatoren wurden entfernt.

- Kapitel und Einträge benötigen eindeutige Kennungen.
- Beschreibungen müssen zur tatsächlich vorhandenen Fiesta-Funktion passen.
- Verweise auf die entfernten eigenen Austauschformate nicht erneut einführen.
- `test_manual` prüft Kennungen, Pflichttexte und Zuordnungen als Teil von CTest.

Die Parser- und Formatabdeckung wird getrennt in `FIESTA_FORMAT_STATUS.md` dokumentiert.
