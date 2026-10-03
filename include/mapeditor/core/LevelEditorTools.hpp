#pragma once
// LevelEditorTools.hpp
// GUI-freie Bausteine fuer die Unreal-aehnlichen Level-Editor-Workflows des Map-Editors:
//   - Snap-Raster in Fiesta-Welteinheiten (SHBD-Zelle 6.25, HTD-Block 50, ...)
//   - Kamera-Lesezeichen (Strg+0..9 setzen, 0..9 anspringen) inkl. Textserialisierung je Karte
//   - Ausgabeprotokoll (Output Log) mit Schweregrad, Wiederholungszaehler und Ringpuffer
//   - Marquee-Auswahl (Inside/Crossing) fuer projizierte Objekt-Bounds
//   - Ausrichten/Verteilen von Positionen (Align/Distribute)
//   - "Spieltest" (Play-in-Editor): Spielerbewegung auf dem echten SHBD-Block&Walk-Gitter
//
// Alles hier ist reiner Editor-Zustand. Nichts davon schreibt Fiesta-Dateien oder leitet
// Fiesta-Laufzeitsemantik ab - die Kollision nutzt ausschliesslich die bereits verifizierte
// Zell-Semantik von WalkGrid (Bit gesetzt = blockiert, 6.25 Welteinheiten je Zelle).

#include "mapeditor/core/WalkGrid.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace theseed::mapeditor::core::level {

// ---------------------------------------------------------------------------------------------
// Snapping
// ---------------------------------------------------------------------------------------------
// Raster in Fiesta-Welteinheiten. 6.25 = eine SHBD-Walk-Zelle (WalkGrid::kCellSize),
// 50 = Standard-Blockgroesse der NA2016-HTD-Heightmaps (z.B. Rou.HTD).
inline constexpr std::array<float, 10> kGridSnapPresets{
    6.25f, 12.5f, 25.0f, 50.0f, 100.0f, 200.0f, 400.0f, 500.0f, 1000.0f, 2500.0f};
inline constexpr std::array<float, 9> kRotationSnapPresets{
    1.0f, 5.0f, 10.0f, 15.0f, 22.5f, 30.0f, 45.0f, 90.0f, 180.0f};
inline constexpr std::array<float, 7> kScaleSnapPresets{
    0.01f, 0.05f, 0.1f, 0.125f, 0.25f, 0.5f, 1.0f};

// Rundet `value` auf das naechste Vielfache von `step`. step <= 0 laesst den Wert unveraendert.
[[nodiscard]] float SnapToStep(float value, float step) noexcept;

// Index des Presets, das `value` am naechsten liegt (fuer Dropdown-Anzeige).
[[nodiscard]] std::size_t NearestPresetIndex(std::span<const float> presets, float value) noexcept;

// ---------------------------------------------------------------------------------------------
// Kamera-Tempo (Unreal: Stufen 1..8)
// ---------------------------------------------------------------------------------------------
inline constexpr int kMinCameraSpeedSetting = 1;
inline constexpr int kMaxCameraSpeedSetting = 8;
inline constexpr int kDefaultCameraSpeedSetting = 4;
// Multiplikator relativ zum Grundtempo: Stufe 4 = 1.0, jede Stufe verdoppelt/halbiert.
[[nodiscard]] float CameraSpeedMultiplier(int setting) noexcept;

// ---------------------------------------------------------------------------------------------
// Kamera-Lesezeichen
// ---------------------------------------------------------------------------------------------
struct CameraBookmark {
    bool valid = false;
    float targetX = 0.0f, targetY = 0.0f, targetZ = 0.0f; // Weltkoordinaten
    float yaw = 0.0f, pitch = 0.0f, distance = 0.0f;
};

struct CameraBookmarkSet {
    static constexpr std::size_t kSlots = 10;
    std::array<CameraBookmark, kSlots> slots{};

    [[nodiscard]] std::size_t ValidCount() const noexcept;
    // Zeilenformat: "bookmark <slot> <tx> <ty> <tz> <yaw> <pitch> <distance>".
    [[nodiscard]] std::string Serialize() const;
    // Robust gegen Kommentare (#), Leerzeilen, unbekannte Zeilen und ungueltige Zahlen.
    [[nodiscard]] static CameraBookmarkSet Parse(std::string_view text);
};

// Dateiname fuer die Lesezeichen einer Karte: nur [A-Za-z0-9_-], Rest -> '_'. Leer -> "_unnamed".
[[nodiscard]] std::string BookmarkFileStem(std::string_view mapStem);

// ---------------------------------------------------------------------------------------------
// Ausgabeprotokoll
// ---------------------------------------------------------------------------------------------
enum class LogSeverity : std::uint8_t { Info = 0, Warning = 1, Error = 2 };

struct LogEntry {
    std::uint64_t sequence = 0;
    double timeSeconds = 0.0;
    LogSeverity severity = LogSeverity::Info;
    std::string category;
    std::string text;
    int repeat = 1;
};

// Heuristische Einstufung einer (deutschen oder englischen) Statusmeldung.
[[nodiscard]] LogSeverity ClassifyLogMessage(std::string_view text);

class EditorLog {
public:
    explicit EditorLog(std::size_t capacity = 2000) : capacity_(capacity == 0 ? 1 : capacity) {}

    // Identische direkt aufeinanderfolgende Meldungen werden zusammengefasst (repeat++).
    const LogEntry& Push(LogSeverity severity, std::string category, std::string text, double timeSeconds);
    void Clear() noexcept { entries_.clear(); }

    [[nodiscard]] const std::deque<LogEntry>& Entries() const noexcept { return entries_; }
    [[nodiscard]] std::size_t Count(LogSeverity severity) const noexcept;
    [[nodiscard]] std::size_t Capacity() const noexcept { return capacity_; }
    // Klartext-Export ("[  12.3s] [Warning] [Map] Text (x3)") fuer Zwischenablage/Datei.
    [[nodiscard]] std::string ExportText() const;

private:
    std::size_t capacity_;
    std::uint64_t nextSequence_ = 1;
    std::deque<LogEntry> entries_;
};

// ---------------------------------------------------------------------------------------------
// Marquee-Auswahl
// ---------------------------------------------------------------------------------------------
struct ScreenPoint { float x = 0.0f, y = 0.0f; };

struct ScreenRect {
    float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f;
    [[nodiscard]] static ScreenRect FromCorners(ScreenPoint a, ScreenPoint b) noexcept;
    [[nodiscard]] bool Contains(ScreenPoint p) const noexcept;
    [[nodiscard]] bool Intersects(const ScreenRect& other) const noexcept;
    [[nodiscard]] float Width() const noexcept { return maxX - minX; }
    [[nodiscard]] float Height() const noexcept { return maxY - minY; }
};

enum class MarqueeMode : std::uint8_t {
    Inside,   // alle projizierten Punkte liegen im Rahmen
    Crossing, // die projizierte Huelle ueberschneidet den Rahmen
};

// Prueft, ob eine Menge projizierter Punkte (z.B. 8 Ecken der Objekt-Bounds) vom Rahmen
// erfasst wird. Leere Mengen werden nie erfasst.
[[nodiscard]] bool MarqueeHits(const ScreenRect& marquee, std::span<const ScreenPoint> projected,
                               MarqueeMode mode) noexcept;

enum class SelectionCombine : std::uint8_t { Replace, Add, Remove };
// Kombiniert eine bestehende Auswahl mit neuen Treffern; Reihenfolge bleibt stabil, keine Duplikate.
[[nodiscard]] std::vector<int> CombineSelection(std::span<const int> current, std::span<const int> hits,
                                                SelectionCombine mode);

// ---------------------------------------------------------------------------------------------
// Ausrichten / Verteilen
// ---------------------------------------------------------------------------------------------
enum class AlignTarget : std::uint8_t { Min, Center, Max, Active };
// Liefert fuer jeden Wert den ausgerichteten Wert. Active = Wert an Index `activeIndex`.
[[nodiscard]] std::vector<float> AlignValues(std::span<const float> values, AlignTarget target,
                                             std::size_t activeIndex = 0);
// Verteilt die Werte gleichmaessig zwischen Minimum und Maximum, wobei die Rangfolge erhalten
// bleibt (kleinster bleibt Minimum, groesster bleibt Maximum). Weniger als 3 Werte: unveraendert.
[[nodiscard]] std::vector<float> DistributeValues(std::span<const float> values);

// ---------------------------------------------------------------------------------------------
// Spieltest (Play-in-Editor) auf dem SHBD-Walk-Gitter
// ---------------------------------------------------------------------------------------------
// true, wenn die Weltposition (x = Ost, z = Server-Y) in einer blockierten oder ausserhalb
// liegenden SHBD-Zelle liegt. Identische Zuordnung wie die NPC-Analyse im Editor: cell = world/6.25.
[[nodiscard]] bool WalkBlockedAtWorld(const WalkGrid& grid, float worldX, float worldZ) noexcept;

// Spiralsuche nach der naechsten begehbaren Zelle (Zellmittelpunkt in Weltkoordinaten).
[[nodiscard]] std::optional<std::array<float, 2>> FindNearestWalkable(
    const WalkGrid& grid, float worldX, float worldZ, int maxRadiusCells = 160);

// Anteil begehbarer Zellen (0..1) - erkennt ein nicht geladenes Gitter (alles blockiert).
[[nodiscard]] double WalkableFraction(const WalkGrid& grid, std::size_t sampleStride = 7);

struct PlaytestConfig {
    // Kollisionsradius des Spielers; 1.5 SHBD-Zellen. Editorwert, keine Clientkonstante.
    float radius = 9.375f;
    // Laufgeschwindigkeit in Welteinheiten/s. Editorwert (einstellbar), nicht aus dem Client belegt.
    float runSpeed = 120.0f;
    float walkSpeedFactor = 0.4f;
    bool collideWithWalkGrid = true;
};

struct PlaytestPawn {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float facingYaw = 0.0f;  // Radiant; 0 = +Z (Norden im Weltraum des Editors)
    bool lastStepBlocked = false;
    float distanceTravelled = 0.0f;
};

struct PlaytestInput {
    float forward = 0.0f; // -1..1, relativ zur Blickrichtung `viewYaw`
    float right = 0.0f;   // -1..1
    bool walk = false;    // langsam gehen statt laufen
    float viewYaw = 0.0f; // Radiant, Richtung "vorwaerts" in Weltkoordinaten: (sin, cos)
};

using BlockedQuery = std::function<bool(float worldX, float worldZ)>;
using HeightQuery = std::function<float(float worldX, float worldZ)>;

// Kreis-gegen-Gitter-Test: Mittelpunkt + 8 Randpunkte.
[[nodiscard]] bool CircleBlocked(const BlockedQuery& blocked, float x, float z, float radius);

// Ein Simulationsschritt. Blockiert die volle Bewegung, wird entlang X bzw. Z geglitten
// (wie bei einer typischen Zellkollision). Hoehe folgt `height`, falls gesetzt.
void StepPlaytest(PlaytestPawn& pawn, const PlaytestInput& input, float dt, const PlaytestConfig& config,
                  const BlockedQuery& blocked, const HeightQuery& height);

} // namespace theseed::mapeditor::core::level
