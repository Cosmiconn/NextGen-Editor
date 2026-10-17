#pragma once
// DataHistory.hpp
// Undo/Redo für die Daten-Editoren (SHN-Tabellen, Quests, Shine-TXT, Portale, NPCs). Eine
// Nutzeraktion ist eine Transaktion aus beliebig vielen Operationen - z. B. dieselbe Zelle in
// Client- und Server-Kopie, das Löschen eines Items in allen Tabellen seiner Familie oder das
// Skalieren einer ganzen Skill-Serie. Rückgängig macht immer die ganze Transaktion.
//
// Operationen sind Paare aus "anwenden" und "zurücknehmen". Sie halten ihre Daten selbst (z. B.
// alten und neuen Zellwert, die gelöschte Zeile samt Position) und greifen über stabile Indizes
// auf die Dokumente zu; der Kern kennt die Editor-Typen nicht und ist dadurch ohne GUI testbar.

#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <vector>

namespace theseed::mapeditor::core {

struct DataOperation {
    std::function<void()> redo; // erneut anwenden (die Änderung ist beim Aufzeichnen schon passiert)
    std::function<void()> undo; // zurücknehmen
};

struct DataTransaction {
    std::string label; // Anzeige, z. B. "Quest 1203 gelöscht"
    std::vector<DataOperation> operations;
};

class DataHistory {
public:
    explicit DataHistory(std::size_t limit = 256) : limit_(limit == 0 ? 1 : limit) {}

    // Beginnt eine Transaktion. Verschachtelte Begin/Commit-Paare werden zu einer äußeren
    // Transaktion zusammengefasst (die Beschriftung der äußersten gilt).
    void Begin(std::string label);
    // Schließt die Transaktion ab. Eine leere Transaktion wird verworfen. Gibt true zurück, wenn
    // damit eine neue Transaktion im Verlauf steht.
    bool Commit();
    // Verwirft die offene Transaktion und nimmt ihre bereits ausgeführten Operationen zurück.
    void Abort();
    [[nodiscard]] bool InTransaction() const noexcept { return depth_ > 0; }

    // Zeichnet eine bereits ausgeführte Änderung auf. Ohne offene Transaktion wird sie zu einer
    // eigenen Transaktion mit der angegebenen Beschriftung. Jede neue Änderung löscht Redo.
    void Record(DataOperation operation, std::string labelIfStandalone = {});

    bool Undo();
    bool Redo();
    [[nodiscard]] bool CanUndo() const noexcept { return !undo_.empty(); }
    [[nodiscard]] bool CanRedo() const noexcept { return !redo_.empty(); }
    [[nodiscard]] std::string UndoLabel() const { return undo_.empty() ? std::string() : undo_.back().label; }
    [[nodiscard]] std::string RedoLabel() const { return redo_.empty() ? std::string() : redo_.back().label; }
    [[nodiscard]] std::size_t UndoCount() const noexcept { return undo_.size(); }
    [[nodiscard]] std::size_t RedoCount() const noexcept { return redo_.size(); }
    // Erhöht sich bei jeder Änderung des Verlaufs (Aufzeichnen, Undo, Redo) - für Caches.
    [[nodiscard]] std::size_t Revision() const noexcept { return revision_; }

    void Clear();

private:
    std::size_t limit_;
    std::deque<DataTransaction> undo_;
    std::deque<DataTransaction> redo_;
    DataTransaction open_;
    int depth_ = 0;
    std::size_t revision_ = 0;
};

} // namespace theseed::mapeditor::core
