// Localization.hpp
// Einfaches Übersetzungssystem für die neue, an den Mockups (NextGen-Editor) orientierte
// Oberfläche: Beschriftungen liegen zunächst auf Deutsch vor, die Sprachumschaltung auf
// Englisch ist aber von Anfang an eingebaut (Nutzerwunsch: "zunächst Deutsch, später
// Englisch als Auswahl hinzufügen"). Die englischen Texte sind bereits funktional befüllt,
// aber NICHT von einem Muttersprachler geprüft - reine Vorbereitung, kein Anspruch auf
// druckreife Übersetzung.
//
// Verwendung: T("nav.project") liefert je nach aktueller Sprache "Projekt" oder "Project".
// Ein fehlender Schlüssel gibt sich selbst zurück (auffälliger als ein leerer String -
// zeigt sofort, wenn beim Hinzufügen neuer UI ein T(...)-Aufruf vergessen wurde, den
// Übersetzungseintrag zu bekommen).
//
// Bewusst NUR für die neue Navigations-/Bildschirm-Ebene (Hauptmenü, Projekt-Konfiguration,
// Map-Editor-Start, Arbeitsbereich-Rahmen) eingeführt. Die bestehenden, tief verschachtelten
// Funktions-Panels (Datei-Import/Export je Modul, siehe DrawToolbar/DrawMenuBar) bleiben in
// dieser Runde als hartcodiertes Deutsch bestehen - eine vollständige Migration aller
// bestehenden Strings wäre ein eigener, risikoarmer Nacharbeits-Schritt (siehe HANDOFF.md).

#pragma once

#include <string>
#include <unordered_map>
#include <utility>

namespace theseed::mapeditor::app {

enum class Language { German, English };

inline Language& CurrentLanguageRef() {
    static Language lang = Language::German; // Default: Deutsch, siehe Nutzeranfrage
    return lang;
}
inline Language CurrentLanguage() { return CurrentLanguageRef(); }
inline void SetLanguage(Language lang) { CurrentLanguageRef() = lang; }

namespace detail {
// Schlüssel -> {Deutsch, Englisch}. Alphabetisch grob nach Bildschirm gruppiert.
inline const std::unordered_map<std::string, std::pair<const char*, const char*>>& TranslationTable() {
    static const std::unordered_map<std::string, std::pair<const char*, const char*>> table = {
        // Obere Navigationsleiste (Projekt-Hub)
        {"nav.project", {"Projekt", "Project"}},
        {"nav.new", {"Neu", "New"}},
        {"nav.open", {"Öffnen", "Open"}},
        {"nav.edit", {"Bearbeiten", "Edit"}},
        {"nav.save", {"Speichern", "Save"}},
        {"nav.credits", {"Credits", "Credits"}},
        {"nav.donate", {"Donate", "Donate"}},
        {"nav.help", {"?", "?"}},
        {"nav.back", {"Zurück", "Back"}},
        {"nav.language", {"Sprache", "Language"}},

        {"card.kfm.title", {"KFM-Animationen", "KFM animations"}},

        // Editor-Karten im Projekt-Hub
        {"card.mapeditor.title", {"MapEditor", "MapEditor"}},
        {"card.mapeditor.f1", {"Hightmap", "Hightmap"}},
        {"card.mapeditor.f2", {"Map Texturen", "Map Textures"}},
        {"card.mapeditor.f3", {"Walk & Block", "Walk & Block"}},
        {"card.mapeditor.f4", {"Objekt Platzierung", "Object Placement"}},
        {"card.mapeditor.f5", {"NPC Platzierung", "NPC Placement"}},
        {"card.mapeditor.f6", {"NPC AI", "NPC AI"}},
        {"card.mapeditor.f7", {"Monster Spawns", "Monster Spawns"}},
        {"card.mapeditor.f8", {"Monster AI", "Monster AI"}},

        {"card.shn.title", {"SHN Editor", "SHN Editor"}},
        {"card.shn.f1", {"Solo Bearbeitung", "Solo Editing"}},
        {"card.shn.f2", {"Paired Bearbeitung", "Paired Editing"}},
        {"card.shn.f3", {"Easy Xp rate up", "Easy Xp rate up"}},
        {"card.shn.f4", {"Easy Price edit", "Easy Price edit"}},

        {"card.quest.title", {"Quest Editor", "Quest Editor"}},
        {"card.quest.f1", {"Easy Quest Create", "Easy Quest Create"}},
        {"card.quest.f2", {"Easy Quest Edit", "Easy Quest Edit"}},

        {"card.interface.title", {"Interface Editor", "Interface Editor"}},
        {"card.interface.f1", {"Interface Edit", "Interface Edit"}},
        {"card.interface.f2", {"Texturing", "Texturing"}},

        {"card.droptable.title", {"Drop Table", "Drop Table"}},
        {"card.droptable.f1", {"Drop Table edit", "Drop Table edit"}},
        {"card.droptable.f2", {"Drop Group edit", "Drop Group edit"}},

        {"card.skill.title", {"Skill + Action", "Skill + Action"}},
        {"card.skill.f1", {"Skill Creator", "Skill Creator"}},
        {"card.skill.f2", {"Skill Editor", "Skill Editor"}},
        {"card.skill.f3", {"Action Editor", "Action Editor"}},

        {"card.comingsoon", {"Dieser Editor ist noch nicht implementiert.",
                              "This editor is not implemented yet."}},
        {"card.start", {"Start", "Start"}},

        // Neues Projekt konfigurieren
        {"newproject.title", {"Neues Projekt konfigurieren", "Configure new project"}},
        {"newproject.name", {"Projekt Name", "Project Name"}},
        {"newproject.projectfolder", {"Projekt Ordner", "Project Folder"}},
        {"newproject.clientfolder", {"Client Ordner", "Client Folder"}},
        {"newproject.serverfolder", {"Server Ordner", "Server Folder"}},
        {"newproject.createsave", {"Projekt erstellen / Speichern", "Create / Save Project"}},
        {"newproject.namemissing", {"Bitte einen Projekt Namen eingeben.", "Please enter a project name."}},
        {"newproject.folderMissing", {"Bitte einen Projekt Ordner wählen.", "Please choose a project folder."}},
        {"newproject.saved", {"Projekt gespeichert.", "Project saved."}},
        {"newproject.savefailed", {"Projekt konnte nicht gespeichert werden: ", "Could not save project: "}},
        {"common.browse", {"Wählen...", "Browse..."}},

        // Map-Editor: New Map / Map Öffnen
        {"mapeditor.title", {"Map-Editor", "Map Editor"}},
        {"mapeditor.newmap", {"New Map", "New Map"}},
        {"mapeditor.openmap", {"Map Öffnen", "Open Map"}},
        {"mapeditor.createnewmap", {"Create New Map", "Create New Map"}},
        {"mapeditor.name", {"Name", "Name"}},
        {"mapeditor.xlength", {"X Länge", "X Length"}},
        {"mapeditor.ybreadth", {"Y Breite", "Y Width"}},
        {"mapeditor.texturelayer", {"Textur Layer", "Texture Layer"}},
        {"mapeditor.createmap", {"Create Map", "Create Map"}},
        {"mapeditor.cancel", {"Cancel", "Cancel"}},
        {"mapeditor.browsemaps", {"Browse Map's", "Browse Maps"}},
        {"mapeditor.open", {"Open", "Open"}},
        {"mapeditor.noclientfolder", {
            "Kein Client-Ordner im aktiven Projekt hinterlegt - bitte zuerst unter 'Neu' "
            "ein Projekt mit Client Ordner anlegen.",
            "No client folder set in the active project - please create a project with a "
            "client folder under 'New' first."}},

        // Map-Editor-Arbeitsbereich
        {"workspace.title", {"Map Editor", "Map Editor"}},
        {"workspace.tab.heightmap", {"Hightmap", "Hightmap"}},
        {"workspace.tab.texturing", {"Texturing", "Texturing"}},
        {"workspace.tab.blockwalk", {"Block/Walk", "Block/Walk"}},
        {"workspace.tab.objects", {"Objects", "Objects"}},
        {"workspace.tab.npcs", {"NPCs", "NPCs"}},
        {"workspace.tab.npcai", {"NPC AI", "NPC AI"}},
        {"workspace.tab.mobs", {"Mobs", "Mobs"}},
        {"workspace.tab.mobai", {"Mob AI", "Mob AI"}},
        {"workspace.tab.portals", {"Portale", "Portals"}},
        {"workspace.file", {"Datei", "File"}},
        {"workspace.save", {"Save", "Save"}},
        {"workspace.saveas", {"Save as", "Save as"}},
        {"workspace.undo", {"Undo", "Undo"}},
        {"workspace.redo", {"Redo", "Redo"}},
        {"workspace.tools", {"Tools/etc", "Tools/etc"}},
        {"workspace.brushes", {"Brushes", "Brushes"}},
        {"workspace.2dview", {"2D View", "2D View"}},
        {"workspace.3dview", {"3D View", "3D View"}},
        {"workspace.notimplemented", {"Noch nicht implementiert.", "Not implemented yet."}},
        {"workspace.savedas", {"Gespeichert als: ", "Saved as: "}},
        {"workspace.centercamera", {"Kamera zentrieren", "Center camera"}},
        {"workspace.wireframe", {"Wireframe (3D)", "Wireframe (3D)"}},
    };
    return table;
}
} // namespace detail

inline const char* T(const std::string& key) {
    const auto& table = detail::TranslationTable();
    const auto it = table.find(key);
    if (it == table.end()) return key.c_str();
    return CurrentLanguage() == Language::German ? it->second.first : it->second.second;
}

} // namespace theseed::mapeditor::app
