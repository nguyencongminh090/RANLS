#pragma once

#include "model/variation_tree.h"
#include "model/board_state.h"

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <vector>

/// Visual tree graph widget — renders VariationTree as connected stone nodes.
/// Inspired by Lizzie / Sabaki style tree visualization.
class TreeNodeView : public Gtk::ScrolledWindow {
public:
    TreeNodeView();

    /// Rebuild the visual tree from the given root and current path.
    void update(const TreeNode *root, const std::vector<Coord> &currentPath);

    /// Signal emitted when a node is clicked (sends path to that node).
    sigc::signal<void(std::vector<Coord>)> signal_node_clicked;
    sigc::signal<void(std::vector<Coord>)> signal_node_hovered;

private:
    /// Layout data computed for each tree node.
    struct NodeLayout {
        const TreeNode *node   = nullptr;
        int             col    = 0;   ///< Column (horizontal position = move depth)
        int             row    = 0;   ///< Row (vertical position, unique per branch)
        bool            onPath = false; ///< Is this node on the current game path?
        std::vector<Coord> path;       ///< Full path from root to this node
    };

    void onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);
    void layoutTree(const TreeNode *node, int depth, int &nextCol,
                    const std::vector<Coord> &currentPath, int pathIdx);

    Gtk::DrawingArea drawArea_;
    std::vector<NodeLayout> nodes_;
    int maxCol_ = 0;
    int maxRow_ = 0;
    int hoverIndex_ = -1;

    static constexpr double kNodeRadius = 7.0;
    static constexpr double kCellW      = 22.0;
    static constexpr double kCellH      = 22.0;
    static constexpr double kPadding    = 12.0;
};
