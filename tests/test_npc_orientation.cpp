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
        const auto projectRoot = root / "_project";
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
