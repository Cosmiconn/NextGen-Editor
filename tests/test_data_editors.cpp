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

#include "mapeditor/core/legacy/QuestText.hpp"

#include <cstdio>
#include <fstream>

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

    std::printf("\n== Quest-Texte in QuestDialog.shn: bearbeiten, anlegen, Klonen mit neuen Text-IDs ==\n");
    {
        core::legacy::ShnFile dialog;
        dialog.encrypted = false;
        dialog.columns = {UIntColumn("ID"), StringColumn("Dialog", 256)};
        for (const auto& [id, text] : std::vector<std::pair<std::uint32_t, const char*>>{
                 {200, "Baby Steps"}, {201, "Erste Schritte"}, {202, "Hallo [NAME]."}, {300, "Fremder Text"}}) {
            core::legacy::ShnRow r;
            r.values = {id, std::string(text)};
            dialog.rows.push_back(std::move(r));
        }
        (void)core::legacy::SaveShnFile(dialog, ressystem / "QuestDialog.shn");
        (void)core::legacy::SaveShnFile(dialog, shine / "QuestDialog.shn");
        EnsureQuestDialogLoaded(st);
        Check(st.questDialogLoaded && st.questDialogDoc >= 0, "QuestDialog.shn als gemeinsames Dokument geladen");

        st.questDataFile.records.clear();
        core::legacy::QuestRecord q1;
        q1.id = 1; q1.title = 200; q1.description = 201;
        q1.start.text = "SAY 202 NPC\nACCEPT\nEND";
        core::legacy::QuestRecord q2 = q1;
        q2.id = 2; q2.description = 0; q2.start.text = "END";
        st.questDataFile.records = {q1, q2};
        st.selectedQuestIdx = 0;
        ++st.questRevision;
        ResyncQuestTracking(st);
        st.dataHistory.Clear();
        Check(QuestTextUseCount(st, 200) == 2 && QuestTextUseCount(st, 202) == 1, "Nutzung: Titel 200 von zwei Quests, SAY 202 von einer");

        Check(SetQuestText(st, 202, "Hallo, Abenteurer!") && QuestTextRaw(st, 202) == "Hallo, Abenteurer!", "SAY-Text geändert");
        Check(DataUndo(st) && QuestTextRaw(st, 202) == "Hallo [NAME].", "Undo stellt den SAY-Text wieder her");
        Check(SetQuestText(st, 250, "Neu") && QuestTextRow(st, 250) >= 0, "fehlende Text-ID: Zeile wird angelegt");
        Check(DataUndo(st) && QuestTextRow(st, 250) < 0, "Undo entfernt die angelegte Textzeile");
        Check(CreateQuestText(st, "Eigener Text") == 301 && QuestTextRaw(st, 301) == "Eigener Text", "neue Text-ID hinter der höchsten (301)");
        Check(DataUndo(st) && QuestTextRow(st, 301) < 0, "Undo entfernt sie wieder");

        // Klonen mit neuen Text-IDs: Kopie der Texte, Verweise umgestellt, ein Undo-Schritt.
        core::legacy::QuestRecord clone = st.questDataFile.records[0];
        clone.id = 3;
        std::set<std::uint32_t> used;
        for (const auto& [id, row] : st.questTextRow) used.insert(static_cast<std::uint32_t>(id));
        const auto remap = core::legacy::AssignNewQuestTextIds(clone, used);
        DataHistoryBegin(st, "Klonen");
        st.questDataFile.records.push_back(clone);
        RecordQuestInserted(st, 2);
        CopyQuestTexts(st, remap);
        DataHistoryCommit(st);
        Check(remap.size() == 3 && clone.title == 301 && clone.description == 302 && clone.start.text == "SAY 303 NPC\nACCEPT\nEND",
              "Klon verweist auf neue Text-IDs 301-303");
        Check(QuestTextRaw(st, 301) == "Baby Steps" && QuestTextRaw(st, 303) == "Hallo [NAME].", "Texte der Vorlage kopiert");
        Check(st.dataHistory.UndoCount() == 1 && DataUndo(st) && st.questDataFile.records.size() == 2 && QuestTextRow(st, 301) < 0,
              "ein Undo nimmt Quest und Texte zurück");
        Check(DataRedo(st) && QuestTextRaw(st, 302) == "Erste Schritte", "Redo stellt beides wieder her");

        Check(SetQuestText(st, 200, "Kleine Schritte") && QuestDialogDirty(st), "Titeltext geändert, Dokument geändert");
        Check(SaveQuestDialogTexts(st), "QuestDialog gespeichert (" + st.statusMessage + ")");
        const auto savedClient = core::legacy::LoadShnFile(base / "Project" / "Client" / "ressystem" / "QuestDialog.shn");
        Check(savedClient && RowOf(*savedClient, "300") >= 0 && savedClient->rows.size() == 7,
              "Client-Kopie im Projekt mit den neuen Zeilen");
        std::filesystem::path serverCopy;
        for (const auto& e : std::filesystem::recursive_directory_iterator(base / "Project" / "Server"))
            if (e.path().filename() == "QuestDialog.shn") serverCopy = e.path();
        const auto savedServer = serverCopy.empty() ? std::expected<core::legacy::ShnFile, std::string>(std::unexpected("fehlt"))
                                                    : core::legacy::LoadShnFile(serverCopy);
        Check(savedServer && savedServer->rows.size() == 7 &&
              core::legacy::ShnValueToString(savedServer->rows[static_cast<std::size_t>(RowOf(*savedServer, "200"))].values[1]) == "Kleine Schritte",
              "bytegleiche Server-Kopie gekoppelt mitgespeichert");
    }

    std::printf("\n== Drop-Gruppen: Katalog folgt den Änderungen, Undo, Speichern ==\n");
    {
        std::filesystem::create_directories(shine / "World");
        {
            std::ofstream out(shine / "World" / "ItemDropGroup.txt", std::ios::binary);
            out << "#TABLE\tItemDropGroup\t;\r\n"
                   "#COLUMNTYPE\tIndex\tString[40]\tWord\tWord\tWord\tWord\tDWORD\t;\r\n"
                   "#COLUMNNAME\tItemGroupIdx\tItemID\tMinQtty\tMaxQtty\tUpgrade00\tUpgrade01\tCheckSum\t;\r\n"
                   "#RECORD\tGem\tSword01\t1\t1\t1000\t0\t1002\t;\r\n"
                   "#RECORD\tEvent01\tNoSuchItem\t1\t1\t1000\t0\t1002\t;\r\n";
        }
        {
            std::ofstream out(shine / "World" / "ItemDropTable.txt", std::ios::binary);
            std::string types = "#COLUMNTYPE\tString[32]\tString[32]", names = "#COLUMNNAME\tMapArea\tMobId";
            std::string rowA = "#RECORD\t-\tSlime", rowB = "#RECORD\t-\tWolf";
            for (int i = 1; i <= 210; ++i) {
                types += "\tString[32]";
                names += "\tDrItem" + std::to_string(i);
                rowA += i == 1 ? "\tGem" : "\t-";
                rowB += i == 7 ? "\tGem" : "\t-";
            }
            out << "#TABLE\tItemGroup\r\n" << types << "\r\n" << names << "\r\n" << rowA << "\r\n" << rowB << "\r\n";
        }
        st.dataHistory.Clear();
        EnsureDropGroupCatalog(st);
        Check(st.dropGroupLoaded && st.dropGroups.HasGroups(), "ItemDropGroup.txt geladen, Katalog aufgebaut");
        Check(st.dropGroups.Check("Gem") == core::legacy::DropSlotStatus::Resolved &&
              st.dropGroups.Check("Event01") == core::legacy::DropSlotStatus::GroupWithoutItems, "Gem löst auf, Event01 ohne Item");

        auto* t = st.dropGroupFile.FindTable("ItemDropGroup");
        const ShineTableAccess access = [&st]() { return st.dropGroupFile.FindTable("ItemDropGroup"); };
        const auto cols = core::legacy::FindDropGroupColumns(*t);
        auto edited = t->records[1];
        edited.values[static_cast<std::size_t>(cols.item)] = "Sword03";
        edited.values[static_cast<std::size_t>(cols.maxQty)] = "4";
        core::legacy::UpdateDropGroupChecksum(cols, edited);
        Check(ReplaceShineRecordWithHistory(st, access, 1, edited, &st.dropGroupDirty, "Event01") && st.dropGroupDirty,
              "Gruppenzeile ersetzt");
        SyncDropGroupCatalog(st);
        Check(st.dropGroups.Check("Event01") == core::legacy::DropSlotStatus::Resolved, "Katalog sieht die neue ItemID sofort");
        Check(DataUndo(st), "Undo");
        SyncDropGroupCatalog(st);
        Check(st.dropGroups.Check("Event01") == core::legacy::DropSlotStatus::GroupWithoutItems &&
              st.dropGroupFile.FindTable("ItemDropGroup")->records[1].values[6] == "1002", "Undo stellt Zeile und Katalog wieder her");
        Check(DataRedo(st) && st.dropGroupFile.FindTable("ItemDropGroup")->records[1].values[6] == "1005", "Redo mit nachgeführter CheckSum");

        Check(InsertShineRecordWithHistory(st, access, 2, core::legacy::MakeDropGroupRecord(*access(), "Neu01", "Sword01", 2, 3),
                                           &st.dropGroupDirty, "neu") == 2, "neue Gruppe angelegt");
        SyncDropGroupCatalog(st);
        Check(st.dropGroups.Check("Neu01") == core::legacy::DropSlotStatus::Resolved, "neue Gruppe im Katalog");

        EnsureDropTableLoaded(st);
        const auto* dropTable = st.dropTableFile.FindTable("ItemGroup");
        Check(dropTable && core::legacy::DropTableRecordsUsingGroup(*dropTable, "Gem") == std::vector<std::size_t>{0, 1},
              "Gem wird von Slime (DrItem1) und Wolf (DrItem7) genutzt");

        Check(SaveDropGroupFile(st) && !st.dropGroupDirty, "Drop-Gruppen gespeichert (" + st.statusMessage + ")");
        std::filesystem::path savedPath;
        for (const auto& e : std::filesystem::recursive_directory_iterator(base / "Project" / "Server"))
            if (e.path().filename() == "ItemDropGroup.txt") savedPath = e.path();
        const auto saved = savedPath.empty() ? std::expected<core::legacy::ShineTextFile, std::string>(std::unexpected("fehlt"))
                                             : core::legacy::LoadShineTextFile(savedPath);
        const auto* st2 = saved ? saved->FindTable("ItemDropGroup") : nullptr;
        Check(st2 && st2->records.size() == 3 && st2->records[1].values[1] == "Sword03" && st2->records[2].values[0] == "Neu01" &&
              st2->records[2].values[6] == "1005", "Projekt-Override enthält Änderung und neue Gruppe (CheckSum 2+3+1000)");
    }

    std::printf("\n== Formular-Editor (Mob-Familie): Client+Server schreiben, Undo, Klonen ==\n");
    {
        auto makeMob = [](std::initializer_list<const char*> extra, std::initializer_list<std::uint32_t> values) {
            core::legacy::ShnFile f;
            f.encrypted = false;
            f.columns = {UIntColumn("ID"), StringColumn("InxName", 32)};
            for (const char* c : extra)
                f.columns.push_back(std::string(c) == "Name" || std::string(c) == "FileName" ? StringColumn(c, 32) : UIntColumn(c));
            for (std::uint32_t id = 1; id <= 2; ++id) {
                core::legacy::ShnRow r;
                r.values = {id, std::string(id == 1 ? "Wolf01" : "Slime01")};
                std::size_t i = 0;
                for (const char* c : extra) {
                    const std::string name = c;
                    if (name == "Name") r.values.push_back(std::string(id == 1 ? "Wolf" : "Slime"));
                    else if (name == "FileName") r.values.push_back(std::string(id == 1 ? "Wolf" : "Slime"));
                    else r.values.push_back(*(values.begin() + static_cast<std::ptrdiff_t>(i)) * id);
                    ++i;
                }
                f.rows.push_back(std::move(r));
            }
            return f;
        };
        (void)core::legacy::SaveShnFile(makeMob({"Name", "Level"}, {0, 5}), ressystem / "MobInfo.shn");
        (void)core::legacy::SaveShnFile(makeMob({"Name", "Level"}, {0, 5}), shine / "MobInfo.shn");
        (void)core::legacy::SaveShnFile(makeMob({"MonEXP"}, {100}), shine / "MobInfoServer.shn");
        (void)core::legacy::SaveShnFile(makeMob({"FileName"}, {0}), ressystem / "MobViewInfo.shn");

        const auto& spec = MobFormSpec();
        const auto docs = ResolveFormDocs(st, spec);
        Check(docs.byTable.size() == 3 && docs.byTable[0].size() == 2 && docs.byTable[1].size() == 1 && docs.byTable[2].size() == 1,
              "MobInfo (Client+Server), MobInfoServer, MobViewInfo aufgelöst");
        st.dataHistory.Clear();
        Check(FormCellRead(st, docs, 0, "Level", 1) == "5" && FormCellRead(st, docs, 1, "MonEXP", 2) == "200", "Werte gelesen");
        Check(FormCellWrite(st, docs, 0, "Level", 1, "7", "Mob #1 · Level") && st.dataHistory.UndoCount() == 1, "Level geschrieben (ein Verlaufseintrag)");
        Check(ShnCellText(ShnDocFile(st, docs.byTable[0][0]), 0, "Level") == "7" && ShnCellText(ShnDocFile(st, docs.byTable[0][1]), 0, "Level") == "7",
              "Client- und Server-Kopie beide geändert");
        Check(!FormCopiesDiffer(st, docs, 0, "Level", 1), "Kopien gleich");
        Check(!FormCellWrite(st, docs, 0, "Level", 1, "7", "gleich"), "gleicher Wert schreibt nichts");
        Check(DataUndo(st) && FormCellRead(st, docs, 0, "Level", 1) == "5" &&
              ShnCellText(ShnDocFile(st, docs.byTable[0][1]), 0, "Level") == "5", "Undo nimmt beide Kopien zurück");
        SetShnCellText(ShnDocFile(st, docs.byTable[0][1]), 0, "Level", "9");
        std::string details;
        Check(FormCopiesDiffer(st, docs, 0, "Level", 1, &details) && details.find("9") != std::string::npos, "Abweichung Client/Server erkannt");
        SetShnCellText(ShnDocFile(st, docs.byTable[0][1]), 0, "Level", "5");

        std::vector<std::string> report;
        st.dataHistory.Clear();
        Check(CloneFormRecord(st, spec, docs, 1, "Slime01", "", report) < 0, "doppelter InxName wird abgelehnt");
        const long long newId = CloneFormRecord(st, spec, docs, 1, "Wolf02", "Grauer Wolf", report);
        Check(newId > 2, "Mob geklont, neue ID " + std::to_string(newId));
        bool everywhere = true;
        for (const auto& copies : docs.byTable)
            for (const int d : copies) everywhere = everywhere && FormRowIn(st, d, newId) >= 0;
        Check(everywhere, "neue Zeile in allen vier Tabellen");
        Check(FormCellRead(st, docs, 0, "Name", newId) == "Grauer Wolf" &&
              ShnCellText(ShnDocFile(st, docs.byTable[0][1]), static_cast<std::size_t>(FormRowIn(st, docs.byTable[0][1], newId)), "Name") == "Grauer Wolf",
              "Name in Client und Server gesetzt");
        Check(FormCellRead(st, docs, 1, "MonEXP", newId) == "100" && FormCellRead(st, docs, 2, "FileName", newId) == "Wolf", "übrige Werte aus der Vorlage");
        Check(st.dataHistory.UndoCount() == 1 && DataUndo(st) && FormRowIn(st, docs.byTable[2][0], newId) < 0 &&
              ShnDocFile(st, docs.byTable[0][0]).rows.size() == 2, "ein Undo entfernt den Klon überall");
        Check(DataRedo(st) && FormCellRead(st, docs, 0, "Name", newId) == "Grauer Wolf", "Redo stellt ihn mit Namen wieder her");
        Check(SaveShnDocument(st, docs.byTable[0][0]), "MobInfo mit Klon besteht die Speicherprüfung (" + st.shnStatus + ")");
    }

    std::printf("\n== Zustände: AbState + AbStateView (Spalte inxName) + SubAbState-Stufen ==\n");
    {
        auto table = [](std::vector<core::legacy::ShnColumn> cols, std::vector<std::vector<core::legacy::ShnValue>> rows) {
            core::legacy::ShnFile f;
            f.encrypted = false;
            f.columns = std::move(cols);
            for (auto& v : rows) { core::legacy::ShnRow r; r.values = std::move(v); f.rows.push_back(std::move(r)); }
            return f;
        };
        const auto abState = table({UIntColumn("ID"), StringColumn("InxName", 32), StringColumn("SubAbState", 32)},
                                   {{std::uint32_t{1}, std::string("StaRedSlash"), std::string("SubStaRedSlash")},
                                    {std::uint32_t{2}, std::string("StaBuckler"), std::string("SubStaBuckler")}});
        (void)core::legacy::SaveShnFile(abState, ressystem / "AbState.shn");
        (void)core::legacy::SaveShnFile(abState, shine / "AbState.shn");
        (void)core::legacy::SaveShnFile(table({UIntColumn("ID"), StringColumn("inxName", 32), UIntColumn("icon")},
                                              {{std::uint32_t{1}, std::string("StaRedSlash"), std::uint32_t{51}},
                                               {std::uint32_t{2}, std::string("StaBuckler"), std::uint32_t{7}}}),
                                        ressystem / "AbStateView.shn");
        const auto sub = table({UIntColumn("ID"), StringColumn("InxName", 32), UIntColumn("Strength")},
                               {{std::uint32_t{20}, std::string("SubStaRedSlash"), std::uint32_t{1}},
                                {std::uint32_t{21}, std::string("SubStaRedSlash"), std::uint32_t{2}},
                                {std::uint32_t{22}, std::string("SubStaBuckler"), std::uint32_t{1}}});
        (void)core::legacy::SaveShnFile(sub, ressystem / "SubAbState.shn");
        (void)core::legacy::SaveShnFile(sub, shine / "SubAbState.shn");

        const auto& spec = AbStateFormSpec();
        const auto docs = ResolveFormDocs(st, spec);
        Check(docs.byTable.size() == 2 && docs.byTable[0].size() == 2 && docs.byTable[1].size() == 1, "AbState (Client+Server) und AbStateView aufgelöst");
        Check(AbStateIdByInx(st, "StaBuckler") == 2 && AbStateIdByInx(st, "gibtsnicht") == -1, "Zustand per InxName gefunden (Sprung aus dem Skill-Editor)");
        Check(FormCellRead(st, docs, 1, "icon", 1) == "51", "AbStateView-Wert gelesen");

        std::vector<std::string> report;
        st.dataHistory.Clear();
        const long long newId = CloneFormRecord(st, spec, docs, 1, "StaRedSlash2", "", report);
        Check(newId > 2 && FormRowIn(st, docs.byTable[1][0], newId) >= 0 &&
              ShnCellText(ShnDocFile(st, docs.byTable[1][0]), static_cast<std::size_t>(FormRowIn(st, docs.byTable[1][0], newId)), "inxName") == "StaRedSlash2",
              "Klon auch in AbStateView (InxName-Spalte 'inxName')");
        Check(FormCellRead(st, docs, 0, "SubAbState", newId) == "SubStaRedSlash", "Klon übernimmt die Stufen-Referenz");
        Check(DataUndo(st) && FormRowIn(st, docs.byTable[1][0], newId) < 0, "Undo entfernt den Klon überall");

        // Stufenwerte: SubAbState-Zeilen per ID, Client und Server gemeinsam.
        dataform::Docs subDocs;
        subDocs.byTable.push_back({SharedShnDoc(st, "SubAbState.shn", EditorState::ShnSource::Client, st.shnClientRoot),
                                   SharedShnDoc(st, "SubAbState.shn", EditorState::ShnSource::Server, st.shnServerRoot)});
        Check(subDocs.byTable[0][0] >= 0 && subDocs.byTable[0][1] >= 0, "SubAbState Client und Server geladen");
        Check(FormCellWrite(st, subDocs, 0, "Strength", 21, "5", "Stufe") &&
              ShnCellText(ShnDocFile(st, subDocs.byTable[0][1]), 1, "Strength") == "5", "Stufenwert in beiden Kopien geschrieben");
        Check(DataUndo(st) && FormCellRead(st, subDocs, 0, "Strength", 21) == "2", "Undo stellt den Stufenwert wieder her");
    }

    std::printf("\n== Ladebildschirme: Zuweisen und Ersetzen mit Undo ==\n");
    {
        // MapViewInfo (Client und Server) und resmenu/loading mit A.png + A.tga (gemeinsam genutzt) und B.png.
        core::legacy::ShnFile mvi;
        mvi.encrypted = false;
        mvi.columns = {UIntColumn("ID"), StringColumn("MapName", 32), StringColumn("Loading", 32), UIntColumn("LoadLocation")};
        for (std::uint32_t i = 1; i <= 2; ++i) {
            core::legacy::ShnRow r;
            r.values = {i, std::string(i == 1 ? "MapOne" : "MapTwo"), std::string("A"), 0u};
            mvi.rows.push_back(std::move(r));
        }
        std::filesystem::create_directories(shine / "View");
        (void)core::legacy::SaveShnFile(mvi, ressystem / "MapViewInfo.shn");
        (void)core::legacy::SaveShnFile(mvi, shine / "View" / "MapViewInfo.shn");
        const auto loading = client / "resmenu" / "loading";
        std::filesystem::create_directories(loading);
        core::RgbaImage grey;
        grey.width = 64;
        grey.height = 48;
        grey.rgba.assign(std::size_t{64} * 48 * 4, 128);
        const auto writeImage = [&](const std::filesystem::path& path, core::ImageContainer container, int tgaBits) {
            core::ImageFormat f;
            f.container = container;
            f.width = 64;
            f.height = 48;
            f.tgaBits = tgaBits;
            f.pngAlpha = false;
            const auto bytes = core::EncodeImage(grey, f);
            std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        };
        writeImage(loading / "A.PNG", core::ImageContainer::Png, 32);
        writeImage(loading / "A.tga", core::ImageContainer::Tga, 16);
        writeImage(loading / "B.png", core::ImageContainer::Png, 32);
        st.interfaceRoot.clear();

        const auto files = LoadingImageFiles(st, "a");
        Check(files.size() == 2 && files[0] == "loading/A.PNG" && files[1] == "loading/A.tga",
              "Ladebild-Varianten gefunden (Groß/Klein egal, PNG vor TGA)");
        st.dataHistory.Clear();
        const int clientDoc = SharedShnDoc(st, "MapViewInfo.shn", EditorState::ShnSource::Client);
        const int serverDoc = SharedShnDoc(st, "MapViewInfo.shn", EditorState::ShnSource::Server);
        Check(clientDoc >= 0 && serverDoc >= 0, "MapViewInfo Client und Server geladen");
        Check(SetMapViewLoading(st, 2, "B") && ShnCellText(ShnDocFile(st, clientDoc), 1, "Loading") == "B" &&
                  ShnCellText(ShnDocFile(st, serverDoc), 1, "Loading") == "B" && ShnCellText(ShnDocFile(st, clientDoc), 0, "Loading") == "A",
              "Zuweisen schreibt nur diese Karte, in Client und Server");
        Check(st.dataHistory.UndoCount() == 1 && DataUndo(st) && ShnCellText(ShnDocFile(st, serverDoc), 1, "Loading") == "A",
              "ein Undo-Schritt stellt beide Seiten wieder her");

        // Ersetzen: Hauptdatei A.PNG, A.tga als weitere Datei im eigenen Format, dazu onApply.
        core::RgbaImage red = grey;
        for (std::size_t i = 0; i < red.rgba.size(); i += 4) red.rgba[i] = 250;
        OpenImageReplaceDialog(st, files[0], "A");
        auto& r = st.imageReplace;
        r.extraTargets = {files[1]};
        r.sourceImage = red;
        r.result = core::BuildReplacement(red, *core::InspectImageFile(loading / "A.PNG"), {}).value();
        r.onApply = [](EditorState& s) { SetMapViewLoading(s, 1, "A"); };
        st.dataHistory.Clear();
        Check(ApplyImageReplacement(st), "Ersetzen übernommen");
        const auto projectLoading = base / "Project" / "Client" / "resmenu" / "loading";
        const auto png = core::InspectImageFile(projectLoading / "A.PNG");
        const auto tga = core::InspectImageFile(projectLoading / "A.tga");
        Check(png && png->container == core::ImageContainer::Png && !png->pngAlpha && tga && tga->container == core::ImageContainer::Tga &&
                  tga->tgaBits == 16 && tga->width == 64,
              "PNG und TGA als Projektkopien, jeweils im Format des Originals");
        const auto tgaImage = core::LoadImageFile(projectLoading / "A.tga");
        Check(tgaImage && tgaImage->rgba[0] > 240 && tgaImage->rgba[3] == 255, "TGA-Variante zeigt das neue Bild (deckend)");
        Check(st.dataHistory.UndoCount() == 1 && DataUndo(st) && !std::filesystem::exists(projectLoading / "A.PNG") &&
                  !std::filesystem::exists(projectLoading / "A.tga"),
              "ein Undo entfernt beide neu angelegten Projektkopien");
        Check(DataRedo(st) && std::filesystem::exists(projectLoading / "A.tga"), "Redo legt sie wieder an");
    }

    std::filesystem::remove_all(base, ec);
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
