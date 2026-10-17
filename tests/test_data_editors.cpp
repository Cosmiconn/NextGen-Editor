// test_data_editors.cpp
// Regressionstests für die gemeinsame Datenschicht der Daten-Editoren: Jede SHN-Datei liegt genau
// einmal im Speicher (EditorState::shnFiles). Früher hielt der NPC-Dialog-Editor eine private
// Kopie von NpcDialogData.shn, während der Kreaturen-Assistent das SHN-Dokument bearbeitete -
// wer zuletzt speicherte, überschrieb die Änderungen des anderen.
//
// main.cpp direkt einbinden (wie test_npc_orientation): braucht GLFW/ImGui/glad zum Kompilieren,
// läuft dank g_disableGlUploads aber ohne OpenGL-Kontext.

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

core::legacy::ShnColumn StringColumn(const char* name, std::uint32_t length) {
    core::legacy::ShnColumn c;
    c.name = name;
    c.type = 9;
    c.length = length;
    c.kind = core::legacy::ShnValueKind::String;
    return c;
}
core::legacy::ShnColumn UIntColumn(const char* name) {
    core::legacy::ShnColumn c;
    c.name = name;
    c.type = 3;
    c.length = 4;
    c.kind = core::legacy::ShnValueKind::UInt32;
    return c;
}

core::legacy::ShnFile MakeNpcDialogData() {
    core::legacy::ShnFile f;
    f.encrypted = false;
    f.columns = {StringColumn("MobIDX", 32), StringColumn("FaceCutFile", 32), StringColumn("Dialog", 256)};
    for (const char* npc : {"NpcAlpha", "NpcBeta"}) {
        core::legacy::ShnRow r;
        r.values = {std::string(npc), std::string("Face01"), std::string("Hallo [NAME].")};
        f.rows.push_back(std::move(r));
    }
    return f;
}

core::legacy::ShnFile MakeTownPortal() {
    core::legacy::ShnFile f;
    f.encrypted = false;
    f.columns = {UIntColumn("Index"), StringColumn("MapName", 32), UIntColumn("X"), UIntColumn("Y")};
    for (std::uint32_t i = 0; i < 2; ++i) {
        core::legacy::ShnRow r;
        r.values = {i, std::string(i == 0 ? "Rou" : "Eld"), 1000u + i, 2000u + i};
        f.rows.push_back(std::move(r));
    }
    return f;
}

int RowOf(const core::legacy::ShnFile& f, const std::string& first) {
    for (std::size_t i = 0; i < f.rows.size(); ++i)
        if (!f.rows[i].values.empty() && core::legacy::ShnValueToString(f.rows[i].values[0]) == first)
            return static_cast<int>(i);
    return -1;
}
} // namespace

int main() {
    g_disableGlUploads = true;
    const auto base = std::filesystem::temp_directory_path() / "nextgen_test_data_editors";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    const auto client = base / "Client";
    const auto server = base / "Server";
    const auto ressystem = client / "ressystem";
    const auto shine = server / "9Data" / "Shine";
    std::filesystem::create_directories(ressystem);
    std::filesystem::create_directories(shine);
    std::filesystem::create_directories(base / "Project");
    if (auto r = core::legacy::SaveShnFile(MakeNpcDialogData(), ressystem / "NpcDialogData.shn"); !r) {
        std::fprintf(stderr, "Fixture: %s\n", r.error().c_str());
        return 2;
    }
    (void)core::legacy::SaveShnFile(MakeTownPortal(), ressystem / "TownPortal.shn");
    (void)core::legacy::SaveShnFile(MakeTownPortal(), shine / "TownPortal.shn");

    EditorState st;
    std::snprintf(st.project.projectFolder, sizeof(st.project.projectFolder), "%s", (base / "Project").string().c_str());
    std::snprintf(st.project.clientFolder, sizeof(st.project.clientFolder), "%s", client.string().c_str());
    std::snprintf(st.project.serverFolder, sizeof(st.project.serverFolder), "%s", server.string().c_str());
    st.project.hasProject = true;
    st.npcDialogRessystemRoot = ressystem.string();
    st.shnClientRoot = ressystem.string();
    st.shnServerRoot = shine.string();
    st.shineTextRoot = shine.string();

    std::printf("== NpcDialogData: Dialog-Editor und Kreaturen-Assistent teilen ein Dokument ==\n");
    {
        EnsureNpcDialogLoaded(st);
        Check(st.npcDialogLoaded && st.npcDialogDoc >= 0, "Dialog-Editor lädt NpcDialogData.shn als SHN-Dokument");
        // Der Kreaturen-Assistent holt sich die Datei über FindOrLoadShnDoc - es muss dasselbe sein.
        const int wizardDoc = FindOrLoadShnDoc(st, "NpcDialogData.shn", EditorState::ShnSource::Client);
        Check(wizardDoc == st.npcDialogDoc, "Kreaturen-Assistent bekommt dasselbe Dokument (keine Zweitkopie)");

        // Dialog-Editor ändert NpcAlpha, der Assistent legt NpcGamma an.
        const int alpha = FindDialogRowForNpc(st, "NpcAlpha");
        Check(alpha >= 0, "Dialogzeile von NpcAlpha gefunden");
        Check(ApplyShnCellText(st, st.npcDialogDoc, alpha, 2, "Neuer Gruß"), "Dialogtext von NpcAlpha geändert");
        const int gamma = CreateNpcDialogRow(st, "NpcGamma");
        Check(gamma >= 0, "Dialogzeile für NpcGamma angelegt");
        Check(FindDialogRowForNpc(st, "NpcGamma") == gamma, "neue Zeile ist über den NPC-Namen auffindbar");

        Check(SaveShnDocument(st, st.npcDialogDoc), "Speichern besteht die Prüfung (" + st.shnStatus + ")");
        const auto saved = core::legacy::LoadShnFile(ShnDocFile(st, st.npcDialogDoc).path);
        Check(saved.has_value(), "gespeicherte Datei lesbar");
        if (saved) {
            const int a = RowOf(*saved, "NpcAlpha");
            Check(a >= 0 && core::legacy::ShnValueToString(saved->rows[static_cast<std::size_t>(a)].values[2]) == "Neuer Gruß",
                  "Änderung des Dialog-Editors steht in der Datei");
            Check(RowOf(*saved, "NpcGamma") >= 0, "Zeile des Assistenten steht in derselben Datei");
            Check(RowOf(*saved, "NpcBeta") >= 0 && saved->rows.size() == 3, "übrige Zeilen unverändert erhalten");
        }
        Check(ShnDocFile(st, st.npcDialogDoc).path.string().find("Project") != std::string::npos,
              "gespeichert wurde in den Projektordner, nicht ins Original");
    }

    std::printf("\n== TownPortal: Portal-Editor und SHN-Editor teilen ein Dokument ==\n");
    {
        EnsureTownPortalLoaded(st);
        Check(st.townPortalLoaded && st.townPortalDoc >= 0, "TownPortal.shn als SHN-Dokument geladen");
        auto& f = ShnDocFile(st, st.townPortalDoc);
        SetShnCellInt(f, 1, FindShnColumnByName(f, "X"), 4242);
        st.townPortalDirty = true;
        SyncTownPortalMarks(st);
        const auto& doc = st.shnFiles[static_cast<std::size_t>(st.townPortalDoc)];
        Check(doc.dirty && doc.cellDirty.size() > 1 && doc.cellDirty[1][2] == 1,
              "Änderung des Portal-Editors ist im SHN-Dokument als geänderte Zelle markiert");
        Check(ValidateShnPatch(st.shnFiles[static_cast<std::size_t>(st.townPortalDoc)]).has_value(),
              "SHN-Editor könnte das Dokument ebenfalls speichern (Prüfung besteht)");
        SaveTownPortalFiles(st);
        Check(!st.townPortalDirty, "TownPortal gespeichert (" + st.statusMessage + ")");
        const auto clientCopy = core::legacy::LoadShnFile(base / "Project" / "Client" / "ressystem" / "TownPortal.shn");
        Check(clientCopy && ShnCellInt(*clientCopy, 1, 2) == 4242, "Client-Kopie im Projekt enthält die Änderung");
        Check(!st.shnFiles[static_cast<std::size_t>(st.townPortalDoc)].dirty, "Dokument danach sauber");
    }

    std::printf("\n== Löschen über die ID-Familie, Verweise, Undo/Redo, Speichern ==\n");
    {
        // Item-Familie: ItemInfo (Client), ItemViewInfo (Client), ItemInfoServer (Server); dazu eine
        // fremde Tabelle, die das Item per InxName nennt.
        auto makeItems = [](bool view) {
            core::legacy::ShnFile f;
            f.encrypted = false;
            f.columns = {UIntColumn("ID"), StringColumn("InxName", 32)};
            if (view) f.columns.push_back(StringColumn("IconFile", 32));
            for (std::uint32_t i = 1; i <= 3; ++i) {
                core::legacy::ShnRow r;
                r.values = {i, std::string("Sword0") + std::to_string(i)};
                if (view) r.values.push_back(std::string("ItemEqu000"));
                f.rows.push_back(r);
            }
            return f;
        };
        (void)core::legacy::SaveShnFile(makeItems(false), ressystem / "ItemInfo.shn");
        (void)core::legacy::SaveShnFile(makeItems(true), ressystem / "ItemViewInfo.shn");
        (void)core::legacy::SaveShnFile(makeItems(false), shine / "ItemInfoServer.shn");
        core::legacy::ShnFile refs;
        refs.encrypted = false;
        refs.columns = {UIntColumn("ID"), StringColumn("RewardItem", 32)};
        refs.rows.push_back({0, {std::uint32_t{7}, std::string("Sword02")}});
        (void)core::legacy::SaveShnFile(refs, ressystem / "RewardList.shn");

        const int info = FindOrLoadShnDoc(st, "ItemInfo.shn", EditorState::ShnSource::Client);
        const int view = FindOrLoadShnDoc(st, "ItemViewInfo.shn", EditorState::ShnSource::Client);
        const int server = FindOrLoadShnDoc(st, "ItemInfoServer.shn", EditorState::ShnSource::Server);
        const int ref = FindOrLoadShnDoc(st, "RewardList.shn", EditorState::ShnSource::Client);
        Check(info >= 0 && view >= 0 && server >= 0 && ref >= 0, "Item-Familie und Verweistabelle geladen");

        const auto refsFound = FindShnRowReferences(st, info, 1);
        Check(refsFound.size() == 1 && refsFound[0].find("RewardList.shn") != std::string::npos,
              "Verweis auf Sword02 in RewardList gefunden");
        st.dataHistory.Clear();
        const int deleted = DeleteShnRecordEverywhere(st, info, 1);
        Check(deleted == 3, "ID 2 in allen drei Familien-Tabellen gelöscht (" + std::to_string(deleted) + ")");
        Check(ShnDocFile(st, view).rows.size() == 2 && ShnDocFile(st, server).rows.size() == 2, "Ansicht und Server ohne ID 2");
        Check(st.dataHistory.UndoCount() == 1, "ein einziger Verlaufseintrag für das Löschen");

        Check(DataUndo(st), "Strg+Z ausgeführt");
        Check(ShnDocFile(st, info).rows.size() == 3 && ShnDocFile(st, view).rows.size() == 3 &&
              ShnDocFile(st, server).rows.size() == 3 && RowOf(ShnDocFile(st, view), "2") == 1,
              "Undo stellt ID 2 in allen Tabellen an derselben Stelle wieder her");
        Check(DataRedo(st) && ShnDocFile(st, info).rows.size() == 2, "Redo löscht erneut");

        Check(SaveShnDocument(st, info), "ItemInfo mit gelöschter Zeile besteht die Speicherprüfung (" + st.shnStatus + ")");
        const auto saved = core::legacy::LoadShnFile(ShnDocFile(st, info).path);
        Check(saved && saved->rows.size() == 2 && RowOf(*saved, "2") < 0 && RowOf(*saved, "3") == 1,
              "gespeicherte Datei ohne ID 2, übrige Zeilen in Reihenfolge");
        Check(st.shnFiles[static_cast<std::size_t>(info)].rowOrigin.empty(), "nach dem Speichern ist der gespeicherte Stand die neue Grundlage");

        // Zelländerung mit Undo.
        Check(ApplyShnCellText(st, view, 0, 2, "ItemEqu001") && DataUndo(st) &&
              core::legacy::ShnValueToString(ShnDocFile(st, view).rows[0].values[2]) == "ItemEqu000",
              "Zelländerung lässt sich rückgängig machen");
        // Neue Zeile mit Undo.
        const auto before = ShnDocFile(st, view).rows.size();
        AddRowWithPropagation(st, view);
        Check(ShnDocFile(st, view).rows.size() == before + 1 && DataUndo(st) && ShnDocFile(st, view).rows.size() == before,
              "neue Zeile lässt sich rückgängig machen");
    }

    std::printf("\n== Quest löschen und klonen mit Undo ==\n");
    {
        st.questDataLoaded = true;
        st.questDataFile.records.clear();
        for (std::uint16_t id : {10, 11, 12}) { core::legacy::QuestRecord q; q.id = id; st.questDataFile.records.push_back(q); }
        st.selectedQuestIdx = 1;
        st.dataHistory.Clear();
        Check(DeleteQuestWithHistory(st, 1) && st.questDataFile.records.size() == 2 && st.questDataFile.records[1].id == 12,
              "Quest 11 gelöscht");
        Check(DataUndo(st) && st.questDataFile.records.size() == 3 && st.questDataFile.records[1].id == 11,
              "Undo stellt Quest 11 an ihrer Stelle wieder her");
        st.questDataFile.records.push_back(st.questDataFile.records[0]);
        st.questDataFile.records.back().id = 13;
        RecordQuestInserted(st, 3);
        Check(DataUndo(st) && st.questDataFile.records.size() == 3, "Klonen lässt sich rückgängig machen");
    }

    std::printf("\n== Shine-TXT-Einträge (Drop-Tabelle) mit Undo ==\n");
    {
        core::legacy::ShineTable table;
        table.name = "ItemGroup";
        for (const char* mob : {"MobA", "MobB"}) { core::legacy::ShineRecord r; r.values = {"-", mob}; table.records.push_back(r); }
        bool dirty = false;
        const ShineTableAccess access = [&table]() { return &table; };
        st.dataHistory.Clear();
        core::legacy::ShineRecord added; added.values = {"-", "MobC"};
        Check(InsertShineRecordWithHistory(st, access, 2, added, &dirty, "neu") == 2 && table.records.size() == 3 && dirty,
              "Drop-Zeile angelegt");
        Check(DeleteShineRecordWithHistory(st, access, 0, &dirty, "weg") && table.records.front().values[1] == "MobB",
              "Drop-Zeile gelöscht");
        Check(DataUndo(st) && table.records.size() == 3 && table.records.front().values[1] == "MobA", "Undo stellt die gelöschte Zeile wieder her");
        Check(DataUndo(st) && table.records.size() == 2, "Undo entfernt die angelegte Zeile");
        Check(DataRedo(st) && table.records.size() == 3 && table.records[2].values[1] == "MobC", "Redo legt sie wieder an");
    }

    std::filesystem::remove_all(base, ec);
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
