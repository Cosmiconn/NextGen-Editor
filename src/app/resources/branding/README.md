# NextGen Branding

Das finale App-/EXE-Icon basiert auf dem freigegebenen metallisch-cyanfarbenen **NG-Monogramm**.

Das bestehende `../nextgen.ico` ist solange **nicht** die endgültige Formreferenz, bis es aus dem freigegebenen NG-Master neu erzeugt wurde.

Pflichtgrößen: 256 / 128 / 96 / 64 / 48 / 32 / 24 / 16 px.  
Kleine Größen: weniger Materialdetails, aber identische NG-Silhouette.

Siehe `docs/ui-vision/mockups/brand-board.svg`.


## Reproduzierbare ICO-Erzeugung

Sobald die freigegebene Rasterquelle im Arbeitsbaum liegt:

```bash
python tools/ui/generate_app_icon.py \
  --source <freigegebenes-ng-masterbild> \
  --output src/app/resources/nextgen.ico \
  --master-output src/app/resources/branding/ng-app-icon-master.png
```

Das Skript entfernt nur den nahezu weißen Hintergrund, zentriert das vorhandene NG-Motiv und erzeugt die festgelegten Windows-Icon-Größen. Es zeichnet **kein neues Logo**.
