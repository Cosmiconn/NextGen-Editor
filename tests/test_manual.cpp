// test_manual.cpp
// Prueft Handbuch, Tooltips und Spalten-Referenz (CHANGELOG [0.44.33]).
// Aufruf: test_manual [pfad/zu/src/app/main.cpp]  - mit Pfad wird zusaetzlich geprueft, dass JEDES Bedienelement im
// Code (UI::Button("...") usw.) einen Tooltip in beiden Sprachen hat.

#include "mapeditor/core/Manual.hpp"

#include <cstdio>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>

using namespace theseed::mapeditor::core::manual;

namespace {
int g_failures = 0;
void Check(bool ok, const std::string& what) {
    if (ok) std::printf("[ok]     %s\n", what.c_str());
    else { std::fprintf(stderr, "[FEHLER] %s\n", what.c_str()); ++g_failures; }
}
} // namespace

int main(int argc, char** argv) {
    std::printf("== Handbuch ==\n");
    Check(Chapters().size() >= 5, "mindestens 5 Kapitel");
    Check(Sections().size() >= 20, "mindestens 20 Abschnitte");
    std::set<std::string> chapterIds, sectionIds;
    for (const auto& c : Chapters()) chapterIds.insert(c.id);
    bool allOk = true;
    for (const auto& s : Sections()) {
        if (!sectionIds.insert(s.id).second) { Check(false, std::string("doppelte Abschnitts-ID ") + s.id); allOk = false; }
        if (!chapterIds.count(s.chapterId)) { Check(false, std::string("unbekanntes Kapitel bei ") + s.id); allOk = false; }
        const std::string tde = s.titleDe, ten = s.titleEn, bde = s.bodyDe, ben = s.bodyEn;
        if (tde.empty() || ten.empty() || bde.size() < 40 || ben.size() < 40) { Check(false, std::string("leerer Titel/Text bei ") + s.id); allOk = false; }
        if (bde == ben) { Check(false, std::string("Text nicht uebersetzt bei ") + s.id); allOk = false; }
        if (bde.find("XXX") != std::string::npos || ben.find("TODO") != std::string::npos) { Check(false, std::string("Platzhalter im Text bei ") + s.id); allOk = false; }
    }
    Check(allOk, "alle Abschnitte: zweisprachig, nicht leer, gueltige Kapitel, eindeutige IDs");
    for (const auto& c : Chapters()) {
        int n = 0;
        for (const auto& s : Sections()) if (std::string(s.chapterId) == c.id) ++n;
        Check(n >= 1 || std::string(c.id) == "reference", std::string("Kapitel hat Abschnitte: ") + c.id);
    }

    std::printf("\n== Suche ==\n");
    auto has = [&](const std::string& q, bool de, const char* id) {
        for (const auto* s : Search(q, de)) if (std::string(s->id) == id) return true;
        return false;
    };
    Check(has("kamera", true, "controls.view3d"), "Suche 'kamera' findet die 3D-Steuerung");
    Check(has("camera", false, "controls.view3d"), "Suche 'camera' (EN) findet die 3D-Steuerung");
    Check(has("skill animation", true, "creators.skills"), "Suche mit zwei Woertern (alle muessen vorkommen)");
    Check(has("walk", false, "map.walk"), "Suche 'walk' (EN)");
    Check(!has("kamera zzzunbekannt", true, "controls.view3d"), "unbekanntes Wort -> kein Treffer");
    Check(Search("", true).size() == Sections().size(), "leere Suche liefert alle Abschnitte");
    {
        const auto hits = Search("zoom", true);
        Check(!hits.empty() && (std::string(hits.front()->id).find("controls") == 0), "Titeltreffer zuerst (zoom -> Steuerung)");
    }

    std::printf("\n== Tooltips ==\n");
    Check(TooltipFor("Button", "Anlegen", true) != nullptr, "Tooltip fuer 'Anlegen' (DE)");
    Check(TooltipFor("Button", "Anlegen", false) != nullptr && std::string(TooltipFor("Button", "Anlegen", false)) != std::string(TooltipFor("Button", "Anlegen", true)), "Tooltip fuer 'Anlegen' (EN) ist uebersetzt");
    Check(TooltipFor("Button", "Importieren##htd", true) != nullptr, "Suffix-Beschriftung 'Importieren##htd'");
    Check(TooltipFor("Button", "Importieren##unbekannt", true) == nullptr, "unbekannter Suffix ohne Basiseintrag -> keiner");
    const char* xi = TooltipFor("InputInt", "X", true);
    const char* xb = TooltipFor("SmallButton", "X", true);
    Check(xi && xb && std::string(xi) != std::string(xb), "gleiche Beschriftung 'X' je Widget-Art unterschiedlich (InputInt/SmallButton)");
    Check(TooltipFor("Button", "T:workspace.save", true) != nullptr, "T()-Schluessel-Tooltip");
    Check(TooltipFor("Button", "gibt es nicht", true) == nullptr, "keine Tooltip-Erfindung fuer Unbekanntes");

    std::printf("\n== Spalten ==\n");
    const ColumnDoc* bp = FindColumn("ItemInfo", "BuyPrice");
    Check(bp && bp->certain, "ItemInfo.BuyPrice ist beschrieben und belegt");
    const ColumnDoc* id = FindColumn("IrgendEineTabelle", "ID");
    Check(id != nullptr, "allgemeine Beschreibung fuer ID in jeder Tabelle");
    const ColumnDoc* dly = FindColumn("ActiveSkill", "DlyTime");
    Check(dly && dly->certain, "ActiveSkill.DlyTime (aus dem Skill-Editor uebernommen)");
    const ColumnDoc* vag = FindColumn("ActiveSkill", "HPRate");
    Check(vag && !vag->certain, "Unsicheres ist als 'vermutet' markiert (ActiveSkill.HPRate)");
    Check(FindColumn("MobViewInfo", "NoSuchColumn") == nullptr, "unbekannte Spalte -> keine Beschreibung");
    bool colsOk = true;
    for (const auto& c : Columns()) if (!*c.de || !*c.en) colsOk = false;
    Check(colsOk, "alle Spalten-Beschreibungen zweisprachig");

    if (argc >= 2) {
        std::printf("\n== Abdeckung: Tooltip fuer JEDES Bedienelement im Code ==\n");
        std::ifstream in(argv[1]);
        std::stringstream buf;
        buf << in.rdbuf();
        const std::string src = buf.str();
        const std::regex re(R"RX(UI::(Button|SmallButton|Checkbox|RadioButton|SliderFloat|SliderInt|InputInt|InputFloat|InputText|InputTextWithHint|Combo|CollapsingHeader|TabItemButton|Selectable|MenuItem)\(\s*(?:T\("([^"]+)"\)|"([^"]+)"))RX");
        std::set<std::string> seen, missing;
        for (auto it = std::sregex_iterator(src.begin(), src.end(), re); it != std::sregex_iterator(); ++it) {
            std::string kind = (*it)[1];
            if (kind == "InputTextWithHint") kind = "InputText";
            const std::string label = (*it)[2].matched ? "T:" + std::string((*it)[2]) : std::string((*it)[3]);
            if (!seen.insert(kind + "|" + label).second) continue;
            if (TooltipFor(kind, label, true) == nullptr || TooltipFor(kind, label, false) == nullptr) missing.insert(kind + ": " + label);
        }
        std::printf("         %zu verschiedene Bedienelemente geprueft\n", seen.size());
        for (const auto& m : missing) std::fprintf(stderr, "         ohne Tooltip: %s\n", m.c_str());
        Check(missing.empty(), "jedes Bedienelement hat einen Tooltip (DE und EN)");
    }
    std::printf("\n%d Fehler.\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
