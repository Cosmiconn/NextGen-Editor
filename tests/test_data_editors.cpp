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

    std::filesystem::remove_all(base, ec);
    if (g_failures) std::fprintf(stderr, "%d Fehler\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
