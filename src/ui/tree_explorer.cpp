#include "tree_explorer.h"
#include "model/move_history.h"

#include <iomanip>
#include <sstream>

static std::string coordStr(Coord c, int boardSize)
{
    if (!c.isValid(boardSize)) return "-";
    return std::string(1, 'A' + c.x) + std::to_string(boardSize - c.y);
}

static std::string formatNodes(int64_t n)
{
    if (n >= 1'000'000) return std::to_string(n / 1'000'000) + "." + std::to_string((n / 100'000) % 10) + "M";
    if (n >= 1'000)     return std::to_string(n / 1'000) + "." + std::to_string((n / 100) % 10) + "K";
    return std::to_string(n);
}

// ═════════════════════════════════════════════════════════════════════════════
TreeExplorer::TreeExplorer()
{
    add_css_class("tree-explorer");
    set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    set_vexpand(true);
    set_hexpand(true);

    store_ = Gio::ListStore<RowData>::create();
    selection_ = Gtk::NoSelection::create(store_);
    columnView_.set_model(selection_);

    // ── Column factories ────────────────────────────────────────────────────
    auto makeColumn = [this](const char *title,
                             const std::function<std::string(const Glib::RefPtr<RowData>&)> &getter) {
        auto factory = Gtk::SignalListItemFactory::create();
        factory->signal_setup().connect([](const Glib::RefPtr<Gtk::ListItem> &item) {
            item->set_child(*Gtk::make_managed<Gtk::Label>());
        });
        factory->signal_bind().connect([getter](const Glib::RefPtr<Gtk::ListItem> &item) {
            auto *label = dynamic_cast<Gtk::Label*>(item->get_child());
            auto  row   = std::dynamic_pointer_cast<RowData>(item->get_item());
            if (label && row) {
                label->set_text(getter(row));
            }
        });
        auto col = Gtk::ColumnViewColumn::create(title, factory);
        col->set_resizable(true);
        columnView_.append_column(col);
    };

    makeColumn("No.",   [](const Glib::RefPtr<RowData> &r) { return r->noStr; });
    makeColumn("Move",  [](const Glib::RefPtr<RowData> &r) { return r->moveStr; });
    makeColumn("Eval",  [](const Glib::RefPtr<RowData> &r) { return r->evalStr; });
    makeColumn("Nodes", [](const Glib::RefPtr<RowData> &r) { return r->nodesStr; });
    makeColumn("Depth", [](const Glib::RefPtr<RowData> &r) { return r->depthStr; });

    // Click handler.
    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_PRIMARY);
    click->signal_released().connect(
        [](int, double, double) {
            // In GTK4 ColumnView with NoSelection, we use position-based lookup.
            // For now, this is a placeholder for a more sophisticated selection model.
        });
    columnView_.add_controller(click);

    set_child(columnView_);
}

void TreeExplorer::update(const MoveHistory &history, const VariationTree &tree, int boardSize)
{
    store_->remove_all();

    const TreeNode *node = tree.root();
    const auto &moves = history.moves();
    int count = history.moveCount();

    std::vector<Coord> currentPath;

    for (int i = 0; i < count; ++i) {
        Coord playedMove = moves[i];
        
        // Advance node if possible
        const TreeNode *nextNode = nullptr;
        if (node) {
            nextNode = node->findChild(playedMove);
        }
        node = nextNode;

        std::string noStr   = std::to_string(i + 1) + ".";
        std::string moveStr = coordStr(playedMove, boardSize);
        std::string evalStr = "-";
        std::string nodesStr = "-";
        std::string depthStr = "-";

        if (node) {
            std::ostringstream evStr;
            evStr << std::fixed << std::setprecision(2) << node->eval;
            evalStr = evStr.str();
            
            if (node->nodes > 0)
                nodesStr = formatNodes(node->nodes);
            if (node->depth > 0)
                depthStr = std::to_string(node->depth);
        }
        
        currentPath.push_back(playedMove);
        
        auto row = RowData::create(
            noStr,
            moveStr,
            evalStr,
            nodesStr,
            depthStr,
            currentPath);

        store_->append(row);
    }
}
