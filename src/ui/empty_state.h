#pragma once

#include <gtkmm.h>
#include <string>

/// Container passthrough for data-driven panels (PVView, TreeExplorer,
/// BottomPanel's logs).
///
/// History: UX-01 added this as an overlay that painted a dimmed "No … yet"
/// placeholder message on top of empty content. UI-08 reversed that decision —
/// an empty panel now just renders empty (clean), with no instructional text.
/// The class and its call sites are kept as a thin wrapper (it only forwards
/// its single child, adding no layout cost) so the reversal stays a
/// text/visibility change and does not disturb panel layout; `setEmpty()` is
/// now a no-op.
class EmptyStateOverlay : public Gtk::Overlay {
public:
    explicit EmptyStateOverlay(const std::string &message);

    /// Sets the widget shown as the overlay's base (real content) layer.
    void setContent(Gtk::Widget &content);

    /// No-op since UI-08 (kept for call-site compatibility).
    void setEmpty(bool empty);
};
