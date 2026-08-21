#include "tree_node_view.h"

#include <algorithm>
#include <cmath>

// ─── Colors ──────────────────────────────────────────────────────────────────
static constexpr double kBgR = 0.17, kBgG = 0.17, kBgB = 0.17;
static constexpr double kLineR = 0.4, kLineG = 0.4, kLineB = 0.4;
static constexpr double kHighlightR = 0.33, kHighlightG = 0.60, kHighlightB = 0.87;

// ═════════════════════════════════════════════════════════════════════════════
TreeNodeView::TreeNodeView()
{
    add_css_class("tree-node-view");
    set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    set_vexpand(true);
    set_hexpand(true);

    drawArea_.set_draw_func(sigc::mem_fun(*this, &TreeNodeView::onDraw));

    // Click handler.
    auto click = Gtk::GestureClick::create();
    click->set_button(GDK_BUTTON_PRIMARY);
    click->signal_released().connect(
        [this](int, double x, double y) {
            // Find the closest node to the click.
            double bestDist = kNodeRadius * 3.0;
            const NodeLayout *bestNode = nullptr;

            for (const auto &nl : nodes_) {
                double nx = kPadding + nl.col * kCellW;
                double ny = kPadding + nl.row * kCellH;
                double dx = x - nx;
                double dy = y - ny;
                double dist = std::sqrt(dx * dx + dy * dy);  // col=X, row=Y
                if (dist < bestDist) {
                    bestDist = dist;
                    bestNode = &nl;
                }
            }

            if (bestNode) {
                signal_node_clicked.emit(bestNode->path);
            }
        });
    drawArea_.add_controller(click);

    // Hover handler.
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect([this](double x, double y) {
        int best = -1;
        double bestDist = kNodeRadius * 2.0;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            const auto &nl = nodes_[i];
            double nx = kPadding + nl.col * kCellW;
            double ny = kPadding + nl.row * kCellH;
            double dx = x - nx;
            double dy = y - ny;
            double dist = std::sqrt(dx * dx + dy * dy);
            if (dist < bestDist) {
                bestDist = dist;
                best = static_cast<int>(i);
            }
        }
        if (best != hoverIndex_) {
            hoverIndex_ = best;
            if (hoverIndex_ >= 0) {
                signal_node_hovered.emit(nodes_[hoverIndex_].path);
            }
            drawArea_.queue_draw();
        }
    });
    motion->signal_leave().connect([this]() {
        hoverIndex_ = -1;
        drawArea_.queue_draw();
    });
    drawArea_.add_controller(motion);

    set_child(drawArea_);
}

// ─── Layout ──────────────────────────────────────────────────────────────────
void TreeNodeView::update(const TreeNode *root, const std::vector<Coord> &currentPath)
{
    nodes_.clear();
    maxCol_ = 0;
    maxRow_ = 0;
    hoverIndex_ = -1;

    if (!root) return;

    int nextCol = 0;
    layoutTree(root, 0, nextCol, currentPath, 0, {}, -1);

    // Size the drawing area to fit all nodes.
    int w = static_cast<int>((maxCol_ + 1) * kCellW + 2 * kPadding);
    int h = static_cast<int>((maxRow_ + 1) * kCellH + 2 * kPadding);
    drawArea_.set_size_request(w, h);
    drawArea_.queue_draw();
}

void TreeNodeView::layoutTree(const TreeNode *node, int depth, int &nextCol,
                               const std::vector<Coord> &currentPath, int pathIdx,
                               const std::vector<Coord> &parentPath, int parentIndex)
{
    // col = horizontal (branch), row = vertical (depth).
    // Main line goes straight down; siblings spread right.
    for (size_t i = 0; i < node->children.size(); ++i) {
        const auto &child = node->children[i];

        // Determine if this child is on the current path.
        bool onPath = false;
        if (pathIdx >= 0 &&
            pathIdx < static_cast<int>(currentPath.size()) &&
            child->move == currentPath[pathIdx]) {
            onPath = true;
        }

        // First child continues on the same column; siblings go to new columns.
        if (i > 0) {
            nextCol++;
        }

        NodeLayout nl;
        nl.node        = child.get();
        nl.col         = nextCol;   // X = branch position
        nl.row         = depth;     // Y = move depth
        nl.onPath      = onPath;
        nl.parentIndex = parentIndex;

        // O(1): parent's path is threaded down through the recursion, so we
        // just copy it and append this move — no scan of nodes_ needed (RT-04:
        // the previous backward scan for the parent's path made the whole
        // layout O(n^2) in tree size).
        nl.path = parentPath;
        nl.path.push_back(child->move);

        maxCol_ = std::max(maxCol_, nl.col);
        maxRow_ = std::max(maxRow_, nl.row);
        nodes_.push_back(nl);

        int childIndex = static_cast<int>(nodes_.size()) - 1;

        // Recurse into this child's subtree, passing this node's own path/index
        // down as the next level's parent path/index.
        layoutTree(child.get(), depth + 1, nextCol, currentPath,
                   onPath ? pathIdx + 1 : -1, nl.path, childIndex);
    }
}

// ─── Drawing ─────────────────────────────────────────────────────────────────
void TreeNodeView::onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int /*width*/, int /*height*/)
{
    // Background handled naturally by GTK native theme.

    if (nodes_.empty()) return;

    // ── Draw connecting lines first ─────────────────────────────────────────
    cr->set_line_width(1.5);

    for (const auto &nl : nodes_) {
        // parentIndex < 0 means the parent is the invisible tree root (no
        // NodeLayout entry to draw a line from) — same case the old
        // `!nl.node->parent` check excluded, but found in O(1) instead of
        // scanning nodes_ for the matching TreeNode* (RT-04).
        if (!nl.node || nl.parentIndex < 0) continue;

        const auto &pl = nodes_[nl.parentIndex];
        double x1 = kPadding + pl.col * kCellW;
        double y1 = kPadding + pl.row * kCellH;
        double x2 = kPadding + nl.col * kCellW;
        double y2 = kPadding + nl.row * kCellH;

        // Highlight line if both nodes are on the current path.
        if (pl.onPath && nl.onPath) {
            cr->set_source_rgb(kHighlightR, kHighlightG, kHighlightB);
            cr->set_line_width(2.0);
        } else {
            cr->set_source_rgb(kLineR, kLineG, kLineB);
            cr->set_line_width(1.0);
        }

        // Draw an elbow: down then across (or direct).
        cr->move_to(x1, y1);
        if (std::abs(x2 - x1) > 1.0) {
            // Elbow: go down to child's row, then across.
            cr->line_to(x1, y2);
            cr->line_to(x2, y2);
        } else {
            cr->line_to(x2, y2);
        }
        cr->stroke();
    }

    // ── Draw nodes ──────────────────────────────────────────────────────────
    for (const auto &nl : nodes_) {
        double cx = kPadding + nl.col * kCellW;
        double cy = kPadding + nl.row * kCellH;

        // Determine stone color based on depth (row 0 = Black's first move).
        bool isBlack = (nl.row % 2 == 0);

        // Node fill.
        if (isBlack) {
            cr->set_source_rgb(0.1, 0.1, 0.1);
        } else {
            cr->set_source_rgb(0.90, 0.90, 0.90);
        }
        cr->arc(cx, cy, kNodeRadius, 0, 2 * M_PI);
        cr->fill();

        bool hovered = (&nl == (hoverIndex_ >= 0 ? &nodes_[hoverIndex_] : nullptr));

        // Node border — highlight if on current path / hovered.
        if (nl.onPath || hovered) {
            cr->set_source_rgb(kHighlightR, kHighlightG, kHighlightB);
            cr->set_line_width(hovered ? 2.8 : 2.0);
        } else {
            cr->set_source_rgba(0.5, 0.5, 0.5, 0.6);
            cr->set_line_width(1.0);
        }
        cr->arc(cx, cy, kNodeRadius, 0, 2 * M_PI);
        cr->stroke();
    }
}
