#pragma once

// UI-10: pure sticky-scroll decision logic for the Engine Log, extracted from
// BottomPanel so it can be unit-tested without a display server.
//
// Background (the bug): while the engine streams analysis lines, BottomPanel
// flushes batched appends on a 50ms timer. The old code decided whether to
// auto-scroll purely from the *live* scroll adjustment sampled on the flush
// tick (`value + page_size >= upper - eps`). During a fast stream the
// adjustment `upper` lags a tick or more behind the text that was just
// inserted, so a genuinely-bottomed view repeatedly reads as "not at bottom"
// and stickiness silently switches itself off — the view drifts above the
// newest line. The fix keeps a remembered stick flag that only an explicit
// user scroll-away clears (see BottomPanel's vadjustment value_changed
// handler), and never trusts a single possibly-stale "not at bottom" read to
// disable it.

namespace sticky_scroll {

/// The three numbers that describe a Gtk::Adjustment's vertical scroll state.
struct ScrollGeometry {
    double value    = 0.0;  ///< current scroll offset (top of viewport)
    double pageSize = 0.0;  ///< visible height
    double upper    = 0.0;  ///< total content height
};

/// True when the viewport bottom edge is at (or within `epsilon` px of) the
/// content bottom. Pure function of the geometry passed in — the caller is
/// responsible for it being current.
inline bool isAtBottom(const ScrollGeometry &g, double epsilon)
{
    return g.value + g.pageSize >= g.upper - epsilon;
}

/// Decide whether flushPending() should scroll the Engine Log to the bottom
/// after inserting this tick's batch.
///
/// `rememberedStick` is BottomPanel's persisted intent: it starts true and is
/// only set false when the user actively scrolls up (detected on a real
/// value_changed event, when the adjustment is trustworthy). `preAppend` is
/// the geometry sampled just before the batch insert.
///
/// We stick if EITHER the remembered intent says so OR the pre-append geometry
/// still shows us at the bottom. We deliberately do NOT let a lone
/// "geometry says not at bottom" reading clear the intent here, because that
/// reading is exactly what goes stale mid-stream.
inline bool shouldStickToBottom(bool rememberedStick, const ScrollGeometry &preAppend,
                                double epsilon)
{
    return rememberedStick || isAtBottom(preAppend, epsilon);
}

/// Recompute the remembered stick flag when the scroll position has actually
/// settled (a Gtk::Adjustment::value_changed event, where the geometry is
/// current and reflects a completed layout). At that moment the geometry IS
/// trustworthy, so we take it at face value: at bottom => keep sticking,
/// scrolled up => stop sticking until the user returns to the bottom.
inline bool updateStickOnSettle(const ScrollGeometry &settled, double epsilon)
{
    return isAtBottom(settled, epsilon);
}

}  // namespace sticky_scroll
