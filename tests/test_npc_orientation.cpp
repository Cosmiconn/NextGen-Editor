// test_npc_orientation.cpp
// Verankert die per Block&Walk-Analyse ermittelte NPC-Blickrichtung (CHANGELOG [0.44.34]) als
// Regressionstest: baut ein winziges, synthetisches WalkGrid mit einer Wand auf einer Seite und
// prueft, dass die AKTUELLEN Standardwerte (state.npcDirSign/npcDirOffsetDeg) den NPC vom
// synthetischen Nachbarn weg und damit von der Wand weg blicken lassen - UND dass die reine
// Formel (NpcYawRadians -> Blickvektor) fuer explizite Vorzeichen/Versatz-Werte arithmetisch
// stimmt. Schlaegt fehl, wenn das Vorzeichen kuenftig versehentlich wieder umgedreht wird.
//
// main.cpp direkt einbinden (wie die Headless-Testharnesses waehrend der Entwicklung) - main.cpp
// braucht GLFW/ImGui/glad zum Kompilieren, ist aber dank g_disableGlUploads GL-frei lauffaehig.

#define main app_main_unused
#include "../src/app/main.cpp"
#undef main

#include <cstdio>

using namespace theseed::mapeditor;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) std::printf("[ok]     %s\n", what.c_str());
    else { std::fprintf(stderr, "[FEHLER] %s\n", what.c_str()); ++g_failures; }
}
} // namespace

int main() {
    g_disableGlUploads = true;

    std::printf("== Formel: NpcYawRadians + Blickvektor (fx=-sin, fz=-cos) ==\n");
    {
        EditorState st;
        // sign=+1, offset=0: Direct=0 -> Blick nach -Z (Suden im Editor-Bezugsrahmen).
        st.npcDirSign = 1;
        st.npcDirOffsetDeg = 0;
        float yaw = NpcYawRadians(st, 0);
        Check(std::fabs(-std::sin(yaw) - 0.0f) < 1e-4f && std::fabs(-std::cos(yaw) - (-1.0f)) < 1e-4f,
              "sign=+1,offset=0,Direct=0 -> Blick (0,-1)");
        // Direct=90 -> Blick nach -X.
        yaw = NpcYawRadians(st, 90);
        Check(std::fabs(-std::sin(yaw) - (-1.0f)) < 1e-4f && std::fabs(-std::cos(yaw) - 0.0f) < 1e-4f,
              "sign=+1,offset=0,Direct=90 -> Blick (-1,0)");
    }

    std::printf("\n== Regression: Standardwerte zeigen vom synthetischen Nachbarn (Wand) weg ==\n");
    {
        // Aufbau: eine 40x40-Zellen-Karte (250x250 Einheiten). Wand = blockierte Zellen fuer x<20,
        // offen fuer x>=20. NPC steht bei x=21 (knapp oestlich der Wand), Direct so gewaehlt, dass er
        // bei KORREKTEM Vorzeichen/Versatz nach Osten (weg von der Wand) blicken sollte: mit
        // sign=+1,offset=0 zeigt Direct=90 nach -X (Westen) und Direct=270 nach +X (Osten) - siehe
        // Formel-Test oben. Der NPC bekommt also Direct=270.
        EditorState st;
        st.walkGrid.Resize(4, 40); // 4 Woerter * 16 Zellen = 64 Spalten, 40 Zeilen - reicht fuer x=0..63
        for (std::uint32_t cz = 0; cz < 40; ++cz)
            for (std::uint32_t cx = 0; cx < 20; ++cx) st.walkGrid.SetCellBlocked(cx, cz, true);
        st.heightmap.Resize(10, 10); st.heightmap.SetBlockSize(25.0f, 25.0f);
        std::snprintf(st.legacySaveStem, sizeof(st.legacySaveStem), "TestMap");

        core::legacy::ShineTable table;
        table.name = "ShineNPC";
        for (const char* col : {"MobName", "Map", "Coord-X", "Coord-Y", "Direct", "NPCMenu", "Role", "RoleArg0"})
            table.columns.push_back({col, "STRING"});
        core::legacy::ShineRecord rec;
        rec.values = {"TestNpc", "TestMap", "131", "125", "270", "1", "Guard", "-"}; // x=21 Zellen*6.25=131.25
        table.records.push_back(rec);
        st.npcTextFile.tables.push_back(std::move(table));
        st.npcTextLoaded = true;

        // Standardwerte (unveraendert aus dem State-Konstruktor) verwenden - genau das soll die Regel verankern.
        Check(st.npcDirSign == 1 && st.npcDirOffsetDeg == 0, "Standardwerte sind sign=+1, offset=0 Grad");

        const float yaw = NpcYawRadians(st, 270);
        const float fx = -std::sin(yaw), fz = -std::cos(yaw);
        Check(fx > 0.9f, "Mit den Standardwerten blickt Direct=270 nach Osten (+X, von der Westwand weg)");

        core::PlacedObject obj;
        ApplyNpcRecordToObject(st, st.npcTextFile.tables[0].records[0], obj);
        const float half = std::atan2(obj.rotY, obj.rotW);
        Check(std::fabs(2.0f * half - yaw) < 1e-3f, "ApplyNpcRecordToObject: Quaternion entspricht NpcYawRadians(Direct)");

        // EstimateNpcOrientation auf genau diesem Szenario: muss auf ein sign/offset kommen, dessen
        // Blickvektor ebenfalls ueberwiegend nach Osten zeigt (perfekte Rekonstruktion ist bei nur 1
        // NPC und 10-Grad-Raster nicht garantiert, wohl aber "ungefaehr richtig").
        EstimateNpcOrientation(st);
        const float yaw2 = NpcYawRadians(st, 270);
        const float fx2 = -std::sin(yaw2);
        Check(fx2 > 0.5f, "EstimateNpcOrientation findet auf dem Testfall ebenfalls 'nach Osten' (fx=" + std::to_string(fx2) + ")");
    }

    std::printf("\n== Objekt-Transform: Mehrfachauswahl, Lock, Pivot ==\n");
    {
        EditorState st;
        core::PlacedObject a; a.modelPath="a.nif"; a.posX=0.0f; a.scale=1.0f;
        core::PlacedObject b; b.modelPath="b.nif"; b.posX=10.0f; b.scale=1.0f;
        st.placementSet.AddObject(a);
        st.placementSet.AddObject(b);
        st.selectedObjects={0,1};
        st.selectedObject=1;
        SyncObjectEditorMetadata(st);

        MoveSelectedObjectsBy(st,5.0f,2.0f,-3.0f);
        Check(std::fabs(st.placementSet.At(0).posX-5.0f)<1e-4f &&
              std::fabs(st.placementSet.At(1).posX-15.0f)<1e-4f,
              "Gruppenbewegung wendet dasselbe Delta auf alle ausgewählten Objekte an");

        st.objectEditorLocked[1]=1;
        MoveSelectedObjectsBy(st,5.0f,0.0f,0.0f);
        Check(std::fabs(st.placementSet.At(0).posX-10.0f)<1e-4f &&
              std::fabs(st.placementSet.At(1).posX-15.0f)<1e-4f,
              "Gesperrte Objekte werden bei Gruppenbewegung nicht verändert");
        st.objectEditorLocked[1]=0;

        const auto pivot=ComputeObjectSelectionPivot(st);
        Check(pivot.valid && std::fabs(pivot.position.x-12.5f)<1e-4f,
              "Gruppenpivot liegt im Mittelpunkt der Auswahl");

        const float half=3.14159265f*0.25f;
        const EditQuat quarterTurn{0.0f,std::sin(half),0.0f,std::cos(half)};
        RotateSelectedObjectsAroundPivot(st,pivot.position,quarterTurn);
        const float dx=st.placementSet.At(0).posX-st.placementSet.At(1).posX;
        const float dz=st.placementSet.At(0).posZ-st.placementSet.At(1).posZ;
        Check(std::fabs(dx)<1e-3f && std::fabs(std::fabs(dz)-5.0f)<1e-3f,
              "90-Grad-Gruppenrotation dreht Positionen um den gemeinsamen Pivot");

        const auto pivot2=ComputeObjectSelectionPivot(st);
        const float beforeDist=std::hypot(st.placementSet.At(0).posX-pivot2.position.x,
                                          st.placementSet.At(0).posZ-pivot2.position.z);
        ScaleSelectedObjectsAroundPivot(st,pivot2.position,2.0f);
        const float afterDist=std::hypot(st.placementSet.At(0).posX-pivot2.position.x,
                                         st.placementSet.At(0).posZ-pivot2.position.z);
        Check(std::fabs(afterDist-beforeDist*2.0f)<1e-3f &&
              std::fabs(st.placementSet.At(0).scale-2.0f)<1e-4f &&
              std::fabs(st.placementSet.At(1).scale-2.0f)<1e-4f,
              "Gruppenskalierung skaliert Abstand zum Pivot und Objektmaß gemeinsam");
    }

    std::printf("\n== AI Workspace: Scan, Dirty-Schutz und Speichern ==\n");
    {
        const auto root=std::filesystem::temp_directory_path()/"nextgen_editor_ai_workspace_test";
        std::error_code ec;
        std::filesystem::remove_all(root,ec);
        std::filesystem::create_directories(root/"LuaScript"/"AIScript",ec);
        std::filesystem::create_directories(root/"MobBehaviorDescript"/"nested",ec);
        {
            std::ofstream out(root/"LuaScript"/"AIScript"/"TestMob.lua",std::ios::binary);
            out << "function AI()\n  return 1\nend\n";
        }
        {
            std::ofstream out(root/"MobBehaviorDescript"/"nested"/"Guard.ps",std::ios::binary);
            out << "state idle\n";
        }

        EditorState st;
        const auto projectRoot = root.parent_path() / "nextgen_editor_ai_workspace_project_test";
        std::filesystem::remove_all(projectRoot, ec);
        std::snprintf(st.project.projectFolder, sizeof(st.project.projectFolder), "%s", projectRoot.string().c_str());
        std::snprintf(st.project.serverFolder, sizeof(st.project.serverFolder), "%s", root.string().c_str());
        st.shnServerRoot=root.string();
        ScanAiWorkspace(st);
        Check(st.aiWorkspaceFiles.size()==2 && st.aiWorkspaceLabels.size()==2,
              "AI Workspace findet Lua- und PineScript-Dateien rekursiv");
        Check(std::any_of(st.aiWorkspaceLabels.begin(),st.aiWorkspaceLabels.end(),
                         [](const std::string& x){ return x.find("Lua")!=std::string::npos && x.find("TestMob.lua")!=std::string::npos; }),
              "Lua-AIScript erhält einen eindeutigen Bibliotheksnamen");
        Check(std::any_of(st.aiWorkspaceLabels.begin(),st.aiWorkspaceLabels.end(),
                         [](const std::string& x){ return x.find("Pine")!=std::string::npos && x.find("Guard.ps")!=std::string::npos; }),
              "PineScript erhält einen eindeutigen Bibliotheksnamen");

        const auto luaPath=root/"LuaScript"/"AIScript"/"TestMob.lua";
        const auto pinePath=root/"MobBehaviorDescript"/"nested"/"Guard.ps";
        Check(LoadAiScriptFile(st,luaPath,"TestMob.lua",false) &&
              st.aiScriptEditorText.find("function AI")!=std::string::npos,
              "AI-Skript wird als Text geladen");
        st.aiScriptEditorText="changed\n";
        st.aiScriptDirty=true;
        Check(LoadAiScriptFile(st,luaPath,"TestMob.lua",false) &&
              st.aiScriptEditorText=="changed\n" && st.aiScriptDirty,
              "Erneute Auswahl desselben AI-Skripts bewahrt den Dirty-Puffer");
        Check(!LoadAiScriptFile(st,pinePath,"Guard.ps",false),
              "Dirty-Schutz verhindert versehentlichen Skriptwechsel");
        Check(SaveAiScript(st) && !st.aiScriptDirty,
              "AI-Skript speichert Änderungen und löscht Dirty-State");
        const auto luaOverride = projectRoot / "Server" / "LuaScript" / "AIScript" / "TestMob.lua";
        std::ifstream saved(luaOverride,std::ios::binary);
        const std::string savedText((std::istreambuf_iterator<char>(saved)),std::istreambuf_iterator<char>());
        Check(savedText=="changed\n","Gespeicherter AI-Text entspricht dem Projekt-Override");
        std::ifstream original(luaPath,std::ios::binary);
        const std::string originalText((std::istreambuf_iterator<char>(original)),std::istreambuf_iterator<char>());
        Check(originalText=="function AI()\n  return 1\nend\n",
              "AI-Save lässt die Server-Quelldatei unverändert");
        Check(LoadAiScriptFile(st,luaPath,"TestMob.lua",false) &&
              st.aiScriptEditorText=="changed\n",
              "AI-Reload bevorzugt den vorhandenen Projekt-Override");
        Check(LoadAiScriptFile(st,pinePath,"Guard.ps",false) &&
              st.aiScriptEditorText.find("state idle")!=std::string::npos,
              "Nach dem Speichern kann auf PineScript gewechselt werden");

        std::filesystem::remove_all(root,ec);
        std::filesystem::remove_all(projectRoot,ec);
    }

    std::printf("\n== SHN-Familien: ausschließlich ID automatisch propagieren ==\n");
    {
        auto makeDoc = [](const char* fileName, EditorState::ShnSource source,
                          const char* namePrefix, std::uint16_t level) {
            EditorState::ShnDocument doc;
            doc.source = source;
            doc.sourcePath = fileName;
            doc.file.path = fileName;
            doc.file.columns = {
                core::legacy::ShnColumn{"ID", 0, 2, core::legacy::ShnValueKind::UInt16, false},
                core::legacy::ShnColumn{"InxName", 0, 32, core::legacy::ShnValueKind::String, false},
                core::legacy::ShnColumn{"Level", 0, 2, core::legacy::ShnValueKind::UInt16, false},
            };
            for (std::uint16_t id = 1; id <= 21; ++id) {
                core::legacy::ShnRow row;
                row.values = {
                    core::legacy::ShnValue{id},
                    core::legacy::ShnValue{std::string(namePrefix) + std::to_string(id)},
                    core::legacy::ShnValue{level},
                };
                doc.file.rows.push_back(std::move(row));
            }
            return doc;
        };

        EditorState st;
        st.shnFiles.push_back(makeDoc("ItemInfo.shn", EditorState::ShnSource::Client, "ClientItem", 7));
        st.shnFiles.push_back(makeDoc("ItemInfoServer.shn", EditorState::ShnSource::Server, "ServerItem", 9));
        st.shnFiles.push_back(makeDoc("OtherInfo.shn", EditorState::ShnSource::Server, "Other", 11));

        AddRowWithPropagation(st, 0);

        Check(st.shnFiles[0].file.rows.size() == 22,
              "Quell-SHN erhält genau eine neue Zeile");
        Check(st.shnFiles[1].file.rows.size() == 22,
              "Verifiziertes Item-Familienmitglied erhält genau eine neue Zeile");
        Check(st.shnFiles[2].file.rows.size() == 21,
              "Gleiche Zeilenzahl/Namensheuristik mutiert keine unverifizierte SHN");

        const auto& peer = st.shnFiles[1];
        Check(ShnCellText(peer.file, 21, "ID") == "22",
              "Nur die verifizierte Familien-ID wird automatisch übernommen");
        Check(ShnCellText(peer.file, 21, "InxName").empty() &&
              ShnCellText(peer.file, 21, "Level") == "0",
              "Gleichnamige Nicht-ID-Spalten werden nicht semantisch erfunden");
        Check(peer.cellStatus.size() == 22 && peer.cellStatus.back().size() == 3 &&
              peer.cellStatus.back()[0] == kShnCellAutoFilled &&
              peer.cellStatus.back()[1] == kShnCellNeedsInput &&
              peer.cellStatus.back()[2] == kShnCellNeedsInput,
              "Peer-Zeile markiert nur ID grün und alle übrigen Zellen als manuell zu prüfen");
    }

    std::printf("\n== Unified Undo/Redo: Chronologie, Objekt-Transaktion und Layer ==\n");
    {
        EditorState st;
        st.heightmap = core::Heightmap(5, 5, 10.0f, 10.0f);
        st.walkGrid.Resize(1, 8, 0); // 0 = alle Testzellen initial begehbar
        st.textureStack = core::TextureLayerStack(4, 4);
        st.textureStack.AddLayer("Base", "base.dds", 1.0f);
        const auto rock = st.textureStack.AddLayer("Rock", "rock.dds", 2.0f);
        st.selectedLayer = static_cast<int>(rock);

        core::PlacedObject obj;
        obj.modelPath = "missing-test-model.nif";
        obj.posX = 10.0f;
        st.placementSet.AddObject(obj);
        st.selectedObjects = {0};
        st.selectedObject = 0;
        SyncObjectEditorMetadata(st);
        ClearMapHistory(st);

        core::BrushSettings brush;
        brush.radius = 15.0f;
        brush.strength = 4.0f;
        auto heightPatch = core::ApplyBrush(st.heightmap, core::BrushMode::Raise, brush, 20.0f, 20.0f);
        PushHeightmapHistory(st, std::move(heightPatch));
        const float raised = st.heightmap.At(2, 2);
        Check(raised > 0.0f, "Terrain-Aktion ist in der gemeinsamen History registriert");

        BeginObjectEditTransaction(st);
        MoveSelectedObjectsBy(st, 3.0f, 0.0f, 0.0f);
        MoveSelectedObjectsBy(st, 2.0f, 0.0f, 0.0f);
        CommitObjectEditTransaction(st);
        Check(std::fabs(st.placementSet.At(0).posX - 15.0f) < 1e-4f,
              "Mehrere Objekt-Deltas einer Drag-Transaktion werden angewendet");
        Check(st.mapHistoryUndo.size() == 2 &&
              st.mapHistoryUndo.back() == MapHistoryDomain::Objects,
              "Kompletter Objekt-Drag erzeugt genau einen chronologischen Undo-Schritt");

        core::WalkUndoPatch walkPatch =
            core::ApplyWalkBitStamp(st.walkGrid, 8.0f, 6.25f, 6.25f, true);
        PushWalkHistory(st, std::move(walkPatch));
        Check(st.walkGrid.CellBlocked(1, 1),
              "Walk-Aktion folgt als dritter History-Schritt");

        Check(UndoMapEdit(st) && !st.walkGrid.CellBlocked(1, 1),
              "Undo #1 nimmt die zuletzt ausgeführte Walk-Aktion zurück");
        Check(std::fabs(st.placementSet.At(0).posX - 15.0f) < 1e-4f,
              "Walk-Undo verändert den davorliegenden Objektzustand nicht");

        Check(UndoMapEdit(st) && std::fabs(st.placementSet.At(0).posX - 10.0f) < 1e-4f,
              "Undo #2 nimmt die gesamte Objekt-Drag-Transaktion in einem Schritt zurück");
        Check(UndoMapEdit(st) && std::fabs(st.heightmap.At(2, 2)) < 1e-4f,
              "Undo #3 nimmt die Terrain-Aktion zurück");

        Check(RedoMapEdit(st) && std::fabs(st.heightmap.At(2, 2) - raised) < 1e-4f,
              "Redo #1 stellt Terrain chronologisch wieder her");
        Check(RedoMapEdit(st) && std::fabs(st.placementSet.At(0).posX - 15.0f) < 1e-4f,
              "Redo #2 stellt die Objekt-Transaktion wieder her");
        Check(RedoMapEdit(st) && st.walkGrid.CellBlocked(1, 1),
              "Redo #3 stellt die Walk-Aktion wieder her");

        const int selectedBefore = st.selectedLayer;
        auto removed = st.textureStack.TakeLayer(rock);
        st.selectedLayer = 0;
        Check(removed.has_value(), "Layer für History-Test gezielt entfernt");
        if (removed)
            RecordTextureLayerRemoved(st, rock, std::move(*removed), selectedBefore);
        Check(st.textureStack.LayerCount() == 1 &&
              st.mapHistoryUndo.back() == MapHistoryDomain::TextureLayers,
              "Layer-Entfernung hängt an derselben chronologischen Timeline");

        Check(UndoMapEdit(st) && st.textureStack.LayerCount() == 2 &&
              st.textureStack.Layer(1).name == "Rock" && st.selectedLayer == 1,
              "Layer-Undo stellt Inhalt, Reihenfolge und Auswahl wieder her");
        Check(CanRedoMapEdit(st), "Layer-Undo erzeugt einen Redo-Schritt");

        MoveSelectedObjectsBy(st, 1.0f, 0.0f, 0.0f);
        Check(!CanRedoMapEdit(st),
              "Neue Kartenaktion nach Undo invalidiert den gesamten Redo-Zweig domainübergreifend");
    }

    std::printf("\n== Map-Scanner: Source + Projekt-only + NIF ohne INI/HTD ==\n");
    {
        const auto base = std::filesystem::temp_directory_path() / "nextgen_nif_only_map_scan_test";
        const auto clientRoot = base / "ClientSource";
        const auto root = clientRoot / "resmap";
        const auto projectRoot = base / "Project";
        std::error_code ec;
        std::filesystem::remove_all(base, ec);

        const auto nifMapDir = root / "field" / "MeshOnly";
        const auto modelDir = root / "object" / "Chair";
        const auto projectIniDir = projectRoot / "Client" / "resmap" / "ProjectOnly";
        const auto projectNifDir = projectRoot / "Client" / "resmap" / "ProjectMesh";
        const auto projectOverrideNifDir =
            projectRoot / "Client" / "resmap" / "field" / "MeshOnly";
        std::filesystem::create_directories(nifMapDir, ec);
        std::filesystem::create_directories(modelDir, ec);
        std::filesystem::create_directories(projectIniDir, ec);
        std::filesystem::create_directories(projectNifDir, ec);
        std::filesystem::create_directories(projectOverrideNifDir, ec);

        const auto sourceNif = nifMapDir / "MeshOnly.nif";
        const auto overrideNif = projectOverrideNifDir / "MeshOnly.nif";
        { std::ofstream out(sourceNif, std::ios::binary); out << "source-nif"; }
        { std::ofstream out(overrideNif, std::ios::binary); out << "override-nif"; }
        { std::ofstream out(modelDir / "Chair.nif", std::ios::binary); out << "not-a-map"; }
        { std::ofstream out(projectIniDir / "ProjectOnly.ini", std::ios::binary); out << ""; }
        { std::ofstream out(projectNifDir / "ProjectMesh.nif", std::ios::binary); out << "project-nif"; }

        auto maps = ScanForMaps(root);
        const auto sourceIt = std::find_if(maps.begin(), maps.end(), [](const DiscoveredMap& m) {
            return m.name == "MeshOnly";
        });
        Check(sourceIt != maps.end() && sourceIt->standaloneNif && !sourceIt->projectOnly &&
              std::filesystem::path(sourceIt->iniPath).filename() == "MeshOnly.nif",
              "Source-Scanner erkennt <field>/<Map>/<Map>.nif ohne INI als NIF-only-Karte");
        Check(std::none_of(maps.begin(), maps.end(), [](const DiscoveredMap& m) {
                  return m.name == "Chair";
              }),
              "Normale resmap-NIF-Modellordner außerhalb field werden nicht als Karten fehlklassifiziert");

        ProjectConfig cfg;
        std::snprintf(cfg.projectFolder, sizeof(cfg.projectFolder), "%s", projectRoot.string().c_str());
        std::snprintf(cfg.clientFolder, sizeof(cfg.clientFolder), "%s", clientRoot.string().c_str());

        const std::size_t added = MergeProjectOnlyMaps(cfg, maps);
        Check(added == 2,
              "Projekt-Katalog ergänzt genau die beiden wirklich projekt-only Karten und dupliziert keinen Source-Override");
        Check(std::count_if(maps.begin(), maps.end(), [](const DiscoveredMap& m) {
                  return m.name == "MeshOnly";
              }) == 1,
              "Vorhandene Source-Karte bleibt einmalig im Katalog; Projekt-Override erzeugt keinen Doppeleintrag");

        const auto projectIni = std::find_if(maps.begin(), maps.end(), [](const DiscoveredMap& m) {
            return m.name == "ProjectOnly";
        });
        Check(projectIni != maps.end() && projectIni->projectOnly && !projectIni->standaloneNif,
              "Projekt-only <Project>/Client/resmap/<Map>/<Map>.ini wird nach Neustart auffindbar");

        const auto projectNif = std::find_if(maps.begin(), maps.end(), [](const DiscoveredMap& m) {
            return m.name == "ProjectMesh";
        });
        Check(projectNif != maps.end() && projectNif->projectOnly && projectNif->standaloneNif,
              "Projekt-only <Project>/Client/resmap/<Map>/<Map>.nif wird als NIF-only-Karte auffindbar");

        Check(PreferProjectOverride(cfg, core::ProjectOutputSide::Client, sourceNif) == overrideNif,
              "Standalone-NIF-Quelle löst einen vorhandenen Projekt-NIF-Override vor der Source auf");

        std::filesystem::remove_all(base, ec);
    }

    std::printf("\n== Map-Projekt-Override: Read-Priorität und Save-Grenzen ==\n");
    {
        const auto root = std::filesystem::temp_directory_path() / "nextgen_editor_map_project_override_test";
        std::error_code ec;
        std::filesystem::remove_all(root, ec);

        const auto clientRoot = root / "ClientSource";
        const auto projectRoot = root / "Project";
        const auto sourceIni = clientRoot / "resmap" / "Rou" / "Rou.ini";
        const auto overrideIni = projectRoot / "Client" / "resmap" / "Rou" / "Rou.ini";
        const auto outsideIni = root / "Outside" / "Rou.ini";
        std::filesystem::create_directories(sourceIni.parent_path(), ec);
        std::filesystem::create_directories(overrideIni.parent_path(), ec);
        std::filesystem::create_directories(outsideIni.parent_path(), ec);
        { std::ofstream out(sourceIni, std::ios::binary); out << "source"; }
        { std::ofstream out(overrideIni, std::ios::binary); out << "override"; }
        { std::ofstream out(outsideIni, std::ios::binary); out << "outside"; }

        EditorState st;
        std::snprintf(st.project.projectFolder, sizeof(st.project.projectFolder), "%s", projectRoot.string().c_str());
        std::snprintf(st.project.clientFolder, sizeof(st.project.clientFolder), "%s", clientRoot.string().c_str());

        Check(LegacyMapWorkingIniPath(st, sourceIni, true) == overrideIni,
              "Map-Open bevorzugt vorhandenen <Project>/Client/resmap-Override");

        auto mappedLegacyOutput = ProjectClientOutputForRequestedPath(st.project, sourceIni);
        Check(mappedLegacyOutput && *mappedLegacyOutput == overrideIni,
              "Legacy-Einzelexport mappt einen Client-Quellpfad ausschließlich in <Project>/Client");
        auto alreadyProjectOutput = ProjectClientOutputForRequestedPath(st.project, overrideIni);
        Check(alreadyProjectOutput && *alreadyProjectOutput == overrideIni,
              "Legacy-Einzelexport akzeptiert ein bereits valides <Project>/Client-Ziel");
        Check(!ProjectClientOutputForRequestedPath(st.project, outsideIni),
              "Legacy-Einzelexport weist beliebige Ziele außerhalb Quelle und Projekt hart ab");
        Check(!ProjectClientOutputForRequestedPath(
                  st.project, clientRoot / "resmap" / ".." / "unsafe.bin"),
              "Legacy-Einzelexport weist explizite '..'-Traversierung hart ab");
        Check(InterfaceProjectOverridePath(st, "../escape.tga").empty(),
              "resmenu-Projektpfad weist '..'-Escape hart ab");

        ProjectConfig nestedCfg = st.project;
        const auto nestedProject = clientRoot / "NestedEditorProject";
        std::snprintf(nestedCfg.projectFolder, sizeof(nestedCfg.projectFolder), "%s",
                      nestedProject.string().c_str());
        std::string configError;
        Check(!SaveProjectConfig(nestedCfg, &configError) &&
              !std::filesystem::exists(nestedProject / "project.tsproj"),
              "Projektkonfiguration darf keinen Projektordner innerhalb der Client-Quelle anlegen");

        std::filesystem::remove(overrideIni, ec);
        Check(LegacyMapWorkingIniPath(st, sourceIni, true) == sourceIni,
              "Map-Open fällt ohne Projekt-Override auf die read-only Quelle zurück");

        std::snprintf(st.legacySaveStem, sizeof(st.legacySaveStem), "Rou");
        std::snprintf(st.legacyMapIniPath, sizeof(st.legacyMapIniPath), "%s", outsideIni.string().c_str());
        st.legacySaveDir[0] = '\0';
        Check(!SaveLegacyMapProject(st),
              "Map-Save weist eine bestehende Quellkarte außerhalb des konfigurierten Client-Roots hart ab");

        st.legacyMapIniPath[0] = '\0';
        std::snprintf(st.legacySaveDir, sizeof(st.legacySaveDir), "%s", (root / "UnsafeOutput").string().c_str());
        Check(!SaveLegacyMapProject(st),
              "Map-Save weist ein explizites Ausgabeverzeichnis außerhalb <Project>/Client hart ab");

        std::filesystem::remove_all(root, ec);
    }

    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
