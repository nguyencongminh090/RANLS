#pragma once

#include "model/variation_tree.h"

#include <gtkmm.h>

/// Hierarchical tree explorer using Gtk::ColumnView.
/// Visualizes GameState::VariationTree with Move | Eval | Nodes | Depth columns.
class TreeExplorer : public Gtk::ScrolledWindow {
public:
    TreeExplorer();

    /// Rebuild the view from the given move history and variation tree.
    void update(const class MoveHistory &history, const class VariationTree &tree, int boardSize);

    /// Signal emitted when the user selects a row (sends the path to that node).
    sigc::signal<void(std::vector<Coord>)> signal_node_selected;

private:
    // Data model for ColumnView.
    struct RowData : public Glib::Object {
        std::string         noStr;
        std::string         moveStr;
        std::string         evalStr;
        std::string         nodesStr;
        std::string         depthStr;
        std::vector<Coord>  path;      ///< Full path from root to this node.
        bool                isCurrent = false; ///< Is this row the current game position?

        static Glib::RefPtr<RowData> create(
            const std::string &no, const std::string &move,
            const std::string &eval, const std::string &nodes,
            const std::string &depth, std::vector<Coord> path,
            bool isCurrent)
        {
            auto obj       = Glib::make_refptr_for_instance<RowData>(new RowData());
            obj->noStr     = no;
            obj->moveStr   = move;
            obj->evalStr   = eval;
            obj->nodesStr  = nodes;
            obj->depthStr  = depth;
            obj->path      = std::move(path);
            obj->isCurrent = isCurrent;
            return obj;
        }
    };

    Gtk::ColumnView                           columnView_;
    Glib::RefPtr<Gio::ListStore<RowData>>     store_;
    Glib::RefPtr<Gtk::SingleSelection>        selection_;

    /// Guards selection_->set_selected() calls made by update() itself (to
    /// reflect the current position) from being mistaken for a user click and
    /// re-emitted as signal_node_selected, which would re-trigger gotoPath.
    bool inUpdate_ = false;
};
