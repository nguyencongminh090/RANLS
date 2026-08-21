#pragma once

#include <gtkmm.h>
#include <string>

/// UX-01: shared "empty state" pattern for data-driven panels that would
/// otherwise render as a blank rectangle before any data exists (fresh
/// launch, or after New Game/undo back to the start — see STATE-01, which
/// clears the underlying data and notifies; this file only reacts to that).
///
/// Two flavors, matching the two kinds of widget in this codebase:
///  - Cairo-drawn widgets (WinGraphView, TreeNodeView) call
///    `EmptyState::drawPlaceholder()` directly from their onDraw() to paint
///    a centered message using the widget's own themed foreground color.
///  - Container widgets wrapping a ListBox/ColumnView/TextView
///    (PVView, TreeExplorer, BottomPanel's logs) use `EmptyStateOverlay`,
///    which overlays a dimmed message Label on top of the real content and
///    toggles its visibility.
namespace EmptyState {

/// Draws `text` centered in `widget`'s (width x height) drawing area using
/// the widget's current themed foreground color at reduced (but still
/// accessible — see docs/fix-log for the contrast calculation) opacity, so
/// it reads correctly in both light and dark GTK themes without a fixed
/// hardcoded color. Call this from onDraw() when the widget has no data.
void drawPlaceholder(const Cairo::RefPtr<Cairo::Context> &cr,
                      Gtk::Widget &widget,
                      int width,
                      int height,
                      const std::string &text);

}  // namespace EmptyState

/// Overlay wrapper: shows a themed, dimmed placeholder message on top of a
/// content widget while the content is empty, and hides it as soon as real
/// data arrives. The content widget keeps its own layout/scrolling; this
/// only adds a floating label above it.
class EmptyStateOverlay : public Gtk::Overlay {
public:
    explicit EmptyStateOverlay(const std::string &message);

    /// Sets the widget shown as the overlay's base (real content) layer.
    void setContent(Gtk::Widget &content);

    /// Toggles the placeholder message's visibility.
    void setEmpty(bool empty);

private:
    Gtk::Label label_;
};
