// UI-08: regression guard that the idle/no-data placeholder TEXT added by
// UX-01 stays removed.
//
// UX-01 wrapped PVView / TreeExplorer / BottomPanel's logs in an
// EmptyStateOverlay that painted a dimmed "No … yet — press Analyze (F5)"
// style Gtk::Label on top of empty content, and drew centered Cairo text in
// WinGraphView / TreeNodeView. UI-08 reversed that: an empty panel renders
// clean, with no instructional text. The axis scaffold in WinGraphView stays
// (structural, not instructional) — that is UI-09's territory, not asserted
// here.
//
// This case only covers the widget-tree (Gtk::Label) flavor — the Cairo-drawn
// text in WinGraphView / TreeNodeView is not reachable through the widget tree
// and is verified by code review (the EmptyState::drawPlaceholder call sites
// were deleted) rather than here.
//
// Links gtkmm; skips cleanly with no display server (see tests/CMakeLists.txt).

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "ui/empty_state.h"
#include "ui/pv_view.h"

#include <string>
#include <vector>

namespace {

/// main() (in test_ui07_pv_view_rows.cpp) has already run gtk_init_check();
/// calling it again is cheap and returns true iff a display server is usable,
/// so these cases self-skip on headless CI just like the UI-07 ones.
bool gtkReady() { return gtk_init_check(); }

/// Every GTK_IS_LABEL text in `root`'s subtree that is currently visible.
std::vector<std::string> visibleLabelTexts(GtkWidget *root)
{
    std::vector<std::string> out;
    if (!root) return out;
    if (GTK_IS_LABEL(root) && gtk_widget_get_visible(root)) {
        const char *t = gtk_label_get_text(GTK_LABEL(root));
        if (t) out.emplace_back(t);
    }
    for (GtkWidget *c = gtk_widget_get_first_child(root); c;
         c = gtk_widget_get_next_sibling(c)) {
        auto sub = visibleLabelTexts(c);
        out.insert(out.end(), sub.begin(), sub.end());
    }
    return out;
}

bool anyContains(const std::vector<std::string> &hay, const std::string &needle)
{
    for (const auto &s : hay)
        if (s.find(needle) != std::string::npos) return true;
    return false;
}

}  // namespace

TEST_CASE("UI-08: EmptyStateOverlay renders no placeholder label when empty")
{
    if (!gtkReady()) return;

    Gtk::Label content{"content"};
    EmptyStateOverlay overlay{"No analysis yet — press Analyze (F5)"};
    overlay.setContent(content);
    overlay.setEmpty(true);

    auto labels = visibleLabelTexts(GTK_WIDGET(overlay.gobj()));

    // The only visible label is the content itself — no injected placeholder.
    CHECK_FALSE(anyContains(labels, "yet"));
    CHECK_FALSE(anyContains(labels, "Analyze"));
    CHECK_FALSE(anyContains(labels, "No analysis"));
}

TEST_CASE("UI-08: an empty PVView shows no placeholder text")
{
    if (!gtkReady()) return;

    PVView pv;
    pv.update({}, 15);  // idle / no-data state

    auto labels = visibleLabelTexts(GTK_WIDGET(pv.gobj()));
    CHECK_FALSE(anyContains(labels, "yet"));
    CHECK_FALSE(anyContains(labels, "press Analyze"));
    CHECK_FALSE(anyContains(labels, "principal variations"));
}
