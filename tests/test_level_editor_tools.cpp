// test_level_editor_tools.cpp
// GUI-freier Test der Level-Editor-Bausteine (Snapping, Kamera-Lesezeichen, Ausgabeprotokoll,
// Marquee, Ausrichten/Verteilen) und des Spieltests auf dem echten NA2016-Gitter Rou.shbd.

#include "mapeditor/core/LevelEditorTools.hpp"
#include "mapeditor/core/WalkGridIO.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <random>

using namespace theseed::mapeditor::core;
using namespace theseed::mapeditor::core::level;

namespace {

int g_failures = 0;

void Check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "[FEHLER] %s\n", what);
        ++g_failures;
    } else {
        std::printf("[ok]     %s\n", what);
    }
}

bool Near(float a, float b, float eps = 1.0e-4f) { return std::abs(a - b) <= eps; }

void TestSnapping() {
    Check(Near(SnapToStep(37.0f, 6.25f), 37.5f), "Snap 37 auf SHBD-Zelle 6.25 -> 37.5");
    Check(Near(SnapToStep(-74.0f, 50.0f), -50.0f), "Snap -74 auf HTD-Block 50 -> -50");
    Check(Near(SnapToStep(12.0f, 0.0f), 12.0f), "Schritt 0 laesst Wert unveraendert");
    Check(kGridSnapPresets[0] == WalkGrid::kCellSize, "Kleinstes Grid-Preset = SHBD-Zellgroesse");
    Check(NearestPresetIndex(kGridSnapPresets, 48.0f) == 3, "Naechstes Preset zu 48 ist 50");
    Check(Near(CameraSpeedMultiplier(4), 1.0f) && Near(CameraSpeedMultiplier(8), 16.0f) &&
          Near(CameraSpeedMultiplier(1), 0.125f) && Near(CameraSpeedMultiplier(99), 16.0f),
          "Kamera-Tempostufen 1..8 verdoppeln je Stufe, Stufe 4 = 1.0, geklemmt");
}

void TestBookmarks() {
    CameraBookmarkSet set;
    set.slots[1] = {true, 1200.5f, 33.25f, 4800.0f, 0.7f, 0.45f, 2500.0f};
    set.slots[9] = {true, -5.0f, 0.0f, 7.5f, -1.25f, 1.2f, 80.0f};
    const std::string text = set.Serialize();
    const auto parsed = CameraBookmarkSet::Parse(text);
    Check(parsed.ValidCount() == 2, "Lesezeichen-Roundtrip: 2 gueltige Slots");
    Check(parsed.slots[1].valid && Near(parsed.slots[1].targetX, 1200.5f) &&
          Near(parsed.slots[1].distance, 2500.0f) && Near(parsed.slots[1].pitch, 0.45f),
          "Lesezeichen-Roundtrip: Werte von Slot 1 erhalten");
    Check(parsed.slots[9].valid && Near(parsed.slots[9].yaw, -1.25f), "Lesezeichen-Roundtrip: Slot 9 erhalten");

    const auto junk = CameraBookmarkSet::Parse(
        "# comment\n\nbookmark 3 1 2 3 0 0 100\r\nbookmark 12 1 2 3 0 0 100\n"
        "bookmark 4 nan 2 3 0 0 100\nbookmark 5 1 2 3 0 0 -5\nfoo bar\nbookmark 6 1 2\n");
    Check(junk.ValidCount() == 1 && junk.slots[3].valid, "Parser ignoriert ungueltige Zeilen, CRLF ok");
    Check(BookmarkFileStem("Rou") == "Rou" && BookmarkFileStem("../a b") == "___a_b" &&
          BookmarkFileStem("") == "_unnamed", "Dateistamm wird bereinigt");
}

void TestLog() {
    EditorLog log(3);
    log.Push(LogSeverity::Info, "Map", "A", 1.0);
    log.Push(LogSeverity::Info, "Map", "A", 2.0);
    Check(log.Entries().size() == 1 && log.Entries().back().repeat == 2, "Wiederholte Meldung wird zusammengefasst");
    log.Push(LogSeverity::Warning, "Map", "B", 3.0);
    log.Push(LogSeverity::Error, "Map", "C", 4.0);
    log.Push(LogSeverity::Info, "Map", "D", 5.0);
    Check(log.Entries().size() == 3 && log.Entries().front().text == "B", "Ringpuffer verwirft aelteste Meldung");
    Check(log.Count(LogSeverity::Error) == 1 && log.Count(LogSeverity::Warning) == 1, "Zaehler je Schweregrad");
    Check(log.ExportText().find("[Error] [Map] C") != std::string::npos, "Textexport enthaelt Schweregrad/Kategorie");
    Check(ClassifyLogMessage("Laden fehlgeschlagen: Rou.shbd") == LogSeverity::Error, "Einstufung: Fehler (de)");
    Check(ClassifyLogMessage("Texture file not found") == LogSeverity::Error, "Einstufung: Fehler (en)");
    Check(ClassifyLogMessage("Terrain-Werkzeuge sind deaktiviert.") == LogSeverity::Warning, "Einstufung: Warnung");
    Check(ClassifyLogMessage("3 Objekt(e) dupliziert.") == LogSeverity::Info, "Einstufung: Info");
}

void TestMarqueeAndSelection() {
    const auto rect = ScreenRect::FromCorners({100, 100}, {10, 20});
    Check(Near(rect.minX, 10) && Near(rect.maxY, 100), "Rechteck aus beliebigen Ecken normalisiert");
    const ScreenPoint inside[] = {{20, 30}, {90, 90}};
    const ScreenPoint partial[] = {{50, 50}, {150, 50}};
    const ScreenPoint outside[] = {{200, 200}, {250, 260}};
    const ScreenPoint around[] = {{0, 0}, {300, 300}};
    Check(MarqueeHits(rect, inside, MarqueeMode::Inside), "Inside: komplett enthalten");
    Check(!MarqueeHits(rect, partial, MarqueeMode::Inside), "Inside: teilweise reicht nicht");
    Check(MarqueeHits(rect, partial, MarqueeMode::Crossing), "Crossing: Ueberschneidung reicht");
    Check(!MarqueeHits(rect, outside, MarqueeMode::Crossing), "Crossing: ausserhalb nicht");
    Check(MarqueeHits(rect, around, MarqueeMode::Crossing), "Crossing: umschliessende Huelle trifft");

    const int cur[] = {1, 2, 3};
    const int hits[] = {3, 4, 4};
    Check((CombineSelection(cur, hits, SelectionCombine::Replace) == std::vector<int>{3, 4}), "Auswahl ersetzen");
    Check((CombineSelection(cur, hits, SelectionCombine::Add) == std::vector<int>{1, 2, 3, 4}), "Auswahl ergaenzen");
    Check((CombineSelection(cur, hits, SelectionCombine::Remove) == std::vector<int>{1, 2}), "Auswahl entfernen");
}

void TestAlignDistribute() {
    const float v[] = {300.0f, 100.0f, 250.0f, 400.0f};
    Check(AlignValues(v, AlignTarget::Min)[2] == 100.0f, "Align Min");
    Check(AlignValues(v, AlignTarget::Max)[1] == 400.0f, "Align Max");
    Check(AlignValues(v, AlignTarget::Center)[0] == 250.0f, "Align Center");
    Check(AlignValues(v, AlignTarget::Active, 2)[3] == 250.0f, "Align auf aktives Objekt");
    const auto d = DistributeValues(v);
    Check(Near(d[1], 100.0f) && Near(d[2], 200.0f) && Near(d[0], 300.0f) && Near(d[3], 400.0f),
          "Distribute: gleichmaessig, Rangfolge erhalten");
}

void TestSyntheticPlaytest() {
    // 4 Woerter breit = 64 Zellen = 400 Einheiten; Wand bei Zellspalte 32 (x = 200..206.25).
    WalkGrid grid(4, 64, 0);
    for (std::uint32_t z = 0; z < 64; ++z) grid.SetCellBlocked(32, z, true);
    Check(!WalkBlockedAtWorld(grid, 100.0f, 100.0f), "Freie Zelle ist begehbar");
    Check(WalkBlockedAtWorld(grid, 203.0f, 100.0f), "Wandzelle blockiert");
    Check(WalkBlockedAtWorld(grid, -1.0f, 100.0f) && WalkBlockedAtWorld(grid, 100.0f, 9999.0f),
          "Ausserhalb des Gitters gilt als blockiert");

    const BlockedQuery blocked = [&](float x, float z) { return WalkBlockedAtWorld(grid, x, z); };
    PlaytestPawn pawn{150.0f, 0.0f, 100.0f};
    PlaytestConfig cfg;
    PlaytestInput east;
    east.forward = 1.0f;
    east.viewYaw = 1.5707963f; // Blick nach +X
    for (int i = 0; i < 200; ++i) StepPlaytest(pawn, east, 1.0f / 60.0f, cfg, blocked, {});
    Check(pawn.x < 200.0f - cfg.radius + 0.01f && pawn.x > 180.0f, "Spieler stoppt vor der SHBD-Wand");
    Check(pawn.lastStepBlocked, "Blockierter Schritt wird gemeldet");

    PlaytestInput diag = east;
    diag.right = -1.0f; // nach links = +Z bei Blick nach +X
    const float zBefore = pawn.z;
    for (int i = 0; i < 30; ++i) StepPlaytest(pawn, diag, 1.0f / 60.0f, cfg, blocked, {});
    Check(pawn.z > zBefore + 10.0f && pawn.x < 200.0f, "Schraeg gegen die Wand: Gleiten entlang Z");

    cfg.collideWithWalkGrid = false;
    for (int i = 0; i < 120; ++i) StepPlaytest(pawn, east, 1.0f / 60.0f, cfg, blocked, {});
    Check(pawn.x > 210.0f, "Ohne Kollision laeuft der Spieler durch");

    const HeightQuery h = [](float x, float) { return x * 0.5f; };
    PlaytestPawn hp{10.0f, 0.0f, 10.0f};
    StepPlaytest(hp, PlaytestInput{}, 0.016f, cfg, blocked, h);
    Check(Near(hp.y, 5.0f), "Hoehe folgt dem Terrain auch im Stillstand");

    const auto spawn = FindNearestWalkable(grid, 203.0f, 50.0f);
    Check(spawn && !WalkBlockedAtWorld(grid, (*spawn)[0], (*spawn)[1]) && std::abs((*spawn)[0] - 203.0f) < 10.0f,
          "Naechste begehbare Zelle neben der Wand gefunden");
    WalkGrid full(2, 8, -1);
    Check(!FindNearestWalkable(full, 10.0f, 10.0f).has_value() && WalkableFraction(full) == 0.0,
          "Komplett blockiertes (nicht geladenes) Gitter wird erkannt");
}

void TestRealRouPlaytest(const std::filesystem::path& shbdPath) {
    // Echte NA2016-Datei: 128 Woerter x 2048 Zeilen (siehe test_walk_grid / docs/MAP_FORMAT.md).
    auto imported = ImportLegacyShbd(shbdPath, 128, 2048);
    Check(imported.has_value(), "Rou.shbd geladen");
    if (!imported) return;
    const WalkGrid& grid = *imported;
    const double fraction = WalkableFraction(grid);
    std::printf("         Rou.shbd begehbarer Anteil (Stichprobe): %.3f\n", fraction);
    Check(fraction > 0.02 && fraction < 0.98, "Rou.shbd enthaelt begehbare und blockierte Zellen");

    // Spawn an der naechsten begehbaren Zelle zur Kartenmitte der genutzten Flaeche.
    const float extent = static_cast<float>(grid.Cols()) * WalkGrid::kCellSize;
    const auto spawn = FindNearestWalkable(grid, extent * 0.25f, extent * 0.25f, 2048);
    Check(spawn.has_value(), "Begehbarer Spawnpunkt auf Rou gefunden");
    if (!spawn) return;

    const BlockedQuery blocked = [&](float x, float z) { return WalkBlockedAtWorld(grid, x, z); };
    PlaytestConfig cfg;
    cfg.radius = 0.0f; // Mittelpunkt-Invariante: der Spieler darf nie in einer blockierten Zelle stehen
    PlaytestPawn pawn{(*spawn)[0], 0.0f, (*spawn)[1]};
    std::mt19937 rng(2016);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    bool everBlockedCell = false;
    int blockedSteps = 0;
    for (int i = 0; i < 20000; ++i) {
        PlaytestInput in;
        in.forward = 1.0f;
        in.right = dist(rng) * 0.3f;
        in.viewYaw = static_cast<float>(i / 90) * 1.3f;
        StepPlaytest(pawn, in, 1.0f / 60.0f, cfg, blocked, {});
        if (pawn.lastStepBlocked) ++blockedSteps;
        if (WalkBlockedAtWorld(grid, pawn.x, pawn.z)) everBlockedCell = true;
    }
    std::printf("         Rou-Spieltest: %.0f Einheiten gelaufen, %d blockierte Schritte\n",
                pawn.distanceTravelled, blockedSteps);
    Check(!everBlockedCell, "Rou-Spieltest: Spieler betritt nie eine blockierte SHBD-Zelle");
    Check(pawn.distanceTravelled > 1000.0f, "Rou-Spieltest: Spieler bewegt sich tatsaechlich");
    Check(blockedSteps > 0, "Rou-Spieltest: echte SHBD-Kollisionen treten auf");
}

} // namespace

int main(int argc, char** argv) {
    TestSnapping();
    TestBookmarks();
    TestLog();
    TestMarqueeAndSelection();
    TestAlignDistribute();
    TestSyntheticPlaytest();
    if (argc > 1) TestRealRouPlaytest(argv[1]);
    else std::printf("[skip]   Rou.shbd-Pfad nicht angegeben\n");

    if (g_failures != 0) {
        std::fprintf(stderr, "%d Test(s) fehlgeschlagen\n", g_failures);
        return 1;
    }
    std::printf("Alle Level-Editor-Tool-Tests bestanden.\n");
    return 0;
}
