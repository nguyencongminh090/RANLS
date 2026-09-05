#include "bottom_panel.h"

namespace {
/// Batch-flush interval for the engine log. Balances "feels live" against
/// collapsing many rapid engine lines into one buffer transaction (RT-02).
constexpr unsigned kFlushIntervalMs = 50;
}  // namespace

BottomPanel::BottomPanel()
{
    add_css_class("bottom-panel");

    // ── Move Log tab ────────────────────────────────────────────────────────
    moveLogView_.set_editable(false);
    moveLogView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    scrolledMoveLog_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    // UI-12: the TextView is the ScrolledWindow's DIRECT child (it implements
    // `Gtk::Scrollable`). Wrapping it in `EmptyStateOverlay` (a plain
    // `Gtk::Overlay`, a no-op passthrough since UI-08) made the ScrolledWindow
    // interpose an implicit `Gtk::Viewport`, so `scrollMoveLogToEnd`'s
    // `scroll_to` was a silent no-op and each new move fell off the bottom —
    // the identical bug fixed for the Engine Log in UI-10 (PR #4).
    scrolledMoveLog_.set_child(moveLogView_);
    append_page(scrolledMoveLog_, "Move Log");

    // UI-12: one permanent right-gravity mark at the end of the Move Log buffer
    // — `scrollMoveLogToEnd` scrolls to THIS instead of creating and immediately
    // deleting a throwaway mark each append. GTK's scroll-to-mark is deferred
    // until after the post-insert relayout; deleting the mark before that pass
    // ran dropped the queued scroll and left new moves off the bottom.
    moveLogEndMark_ = moveLogView_.get_buffer()->create_mark(
        moveLogView_.get_buffer()->end(), /*left_gravity=*/false);

    // ── Engine Log tab ──────────────────────────────────────────────────────
    // UI-05: the TextView holds ONLY the raw engine payload (one line per
    // logical engine line). The direction/category tag (SEND/MESSAGE/…)
    // is painted by a sibling fixed-width DrawingArea gutter, like a text
    // editor's line-number column — visible and aligned, but not part of the
    // selectable text, so selecting rows and copying yields payload only.
    // The gutter paints each tag at its buffer line's live y-position, so it
    // cannot desync from its line on wrap/resize (replaces both the old
    // dual-TextView gutter split and the inline tagged-prefix approach — RT-02).
    engineLogView_.set_editable(false);
    engineLogView_.set_cursor_visible(false);
    engineLogView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    engineLogView_.add_css_class("monospace");

    // UI-10: one permanent right-gravity mark at the end of the buffer. The
    // flush path scrolls to THIS mark rather than creating and immediately
    // deleting a throwaway one each tick — deleting the mark before GTK's
    // post-relayout deferred-scroll pass ran was dropping the queued scroll,
    // so a fast stream left the newest line off-screen.
    engineLogEndMark_ = engineLogView_.get_buffer()->create_mark(
        engineLogView_.get_buffer()->end(), /*left_gravity=*/false);

    gutterArea_.add_css_class("engine-gutter");
    gutterArea_.set_valign(Gtk::Align::FILL);
    gutterArea_.set_draw_func(sigc::mem_fun(*this, &BottomPanel::drawGutter));

    scrolledEngineLog_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    // UI-10: the TextView is the ScrolledWindow's DIRECT child. It must be —
    // `Gtk::TextView` implements `Gtk::Scrollable`, so the ScrolledWindow drives
    // the TextView's own vertical scroll and `engineLogView_.scroll_to(mark,…)`
    // actually moves the view. Wrapping it in `EmptyStateOverlay` (a plain
    // `Gtk::Overlay`, not `Gtk::Scrollable` — and a no-op passthrough since
    // UI-08) made the ScrolledWindow insert an implicit `Gtk::Viewport`: the
    // Viewport then scrolled while the TextView sat at full height inside it,
    // so every `scroll_to` on the TextView was a silent no-op and the log
    // stayed pinned to the FIRST line during analysis.
    scrolledEngineLog_.set_child(engineLogView_);
    scrolledEngineLog_.set_hexpand(true);
    scrolledEngineLog_.set_vexpand(true);

    // Repaint the gutter whenever the log scrolls or relayouts (resize/wrap
    // change the adjustment's range even when its value is unchanged).
    if (auto vadj = scrolledEngineLog_.get_vadjustment()) {
        vadj->signal_value_changed().connect([this] {
            gutterArea_.queue_draw();
            // UI-10: a value change we did NOT cause means the user moved the
            // view (wheel, scrollbar drag, keyboard) — re-derive the "follow
            // the tail" intent from where they landed: at the bottom => keep
            // sticking, scrolled up => stop until they come back down.
            //
            // While `programmaticScroll_` is set, the change is our own
            // auto-scroll or the front-trim (RT-02) shifting content — those
            // transiently read "not at bottom" against a mid-flush layout and
            // must NOT be mistaken for the user scrolling away (doing so was
            // latching stickiness permanently off).
            if (programmaticScroll_)
                return;
            stickToBottom_ =
                sticky_scroll::updateStickOnSettle(engineLogGeometry(), kBottomEpsilon);
        });
        vadj->signal_changed().connect([this] { gutterArea_.queue_draw(); });
    }

    engineLogRow_.append(gutterArea_);
    engineLogRow_.append(scrolledEngineLog_);
    engineLogRow_.set_hexpand(true);
    engineLogRow_.set_vexpand(true);

    // Batch flush: accumulate appendSend/appendRecv calls in pendingAppend_
    // and apply them to the GTK buffer in one transaction per tick, instead
    // of one transaction (plus a forced scroll/relayout) per line (RT-02).
    flushTimerConn_ = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &BottomPanel::flushPending), kFlushIntervalMs);

    // Command entry.
    commandEntry_.set_placeholder_text("Type command…");
    commandEntry_.signal_activate().connect([this]() {
        auto text = commandEntry_.get_text();
        if (!text.empty()) {
            commandHistory_.push_back(std::string(text));
            historyIdx_ = -1;
            commandEntry_.set_text("");

            std::string cmd(text);
            // Forward to MainWindow/command layer.
            signal_command_sent.emit(cmd);
        }
    });

    // Up/Down arrow for command history.
    auto keyCtrl = Gtk::EventControllerKey::create();
    keyCtrl->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) -> bool {
            if (commandHistory_.empty()) return false;

            if (keyval == GDK_KEY_Up) {
                if (historyIdx_ < 0)
                    historyIdx_ = static_cast<int>(commandHistory_.size()) - 1;
                else if (historyIdx_ > 0)
                    historyIdx_--;
                commandEntry_.set_text(commandHistory_[historyIdx_]);
                commandEntry_.set_position(-1);
                return true;
            }
            if (keyval == GDK_KEY_Down) {
                if (historyIdx_ >= 0 &&
                    historyIdx_ < static_cast<int>(commandHistory_.size()) - 1) {
                    historyIdx_++;
                    commandEntry_.set_text(commandHistory_[historyIdx_]);
                } else {
                    historyIdx_ = -1;
                    commandEntry_.set_text("");
                }
                commandEntry_.set_position(-1);
                return true;
            }
            return false;
        },
        false);
    commandEntry_.add_controller(keyCtrl);

    engineLogBox_.append(engineLogRow_);
    engineLogBox_.append(commandEntry_);
    append_page(engineLogBox_, "Engine Log");

    set_size_request(-1, 120);
}

const char *BottomPanel::gutterColorForKind(LogTagKind tag)
{
    switch (tag) {
        case LogTagKind::Send:        return "#6ab0f3";  // Blue
        case LogTagKind::RecvMessage: return "#c678dd";  // Purple
        case LogTagKind::RecvCoord:   return "#e5c07b";  // Yellow/Orange
        case LogTagKind::RecvError:   return "#e06c75";  // Red
        case LogTagKind::RecvOutput:
        default:                      return "#8cc265";  // Green
    }
}

void BottomPanel::drawGutter(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height)
{
    // Paint the tags in the exact font (family + size) the Engine Log's
    // TextView resolved from its `monospace` CSS class, so a gutter tag lines
    // up visually with the log row it labels instead of guessing "Monospace 11".
    const Pango::FontDescription fontDesc =
        engineLogView_.get_pango_context()->get_font_description();

    // Fixed column width: wide enough for the longest tag, computed once.
    if (gutterWidth_ == 0) {
        auto probe = gutterArea_.create_pango_layout("MESSAGE");
        probe->set_font_description(fontDesc);
        int w = 0, h = 0;
        probe->get_pixel_size(w, h);
        gutterWidth_ = w + 12;
        gutterArea_.set_size_request(gutterWidth_, -1);
        return;  // a repaint follows the resize
    }

    const auto &lines = engineLogModel_.lines();
    if (lines.empty())
        return;

    auto  vadj      = scrolledEngineLog_.get_vadjustment();
    double scrollY  = vadj ? vadj->get_value() : 0.0;

    // First buffer line at/above the top of the viewport, then walk down
    // until we're past the bottom. O(visible lines), not O(buffer).
    Gtk::TextBuffer::iterator iter;
    int lineTop = 0;
    engineLogView_.get_line_at_y(iter, static_cast<int>(scrollY), lineTop);

    for (; !iter.is_end(); iter.forward_line()) {
        int yb = 0, lh = 0;
        engineLogView_.get_line_yrange(iter, yb, lh);

        const double y = yb - scrollY;
        if (y > height)
            break;
        if (y + lh < 0)
            continue;

        const std::size_t idx = static_cast<std::size_t>(iter.get_line());
        if (idx >= lines.size())
            break;  // payload contained a newline — stop rather than misalign

        const auto &line   = lines[idx];
        auto        layout = gutterArea_.create_pango_layout(line.prefix);
        layout->set_font_description(fontDesc);

        Gdk::RGBA rgba;
        rgba.set(gutterColorForKind(line.tag));
        cr->set_source_rgb(rgba.get_red(), rgba.get_green(), rgba.get_blue());
        cr->move_to(6.0, y);
        layout->show_in_cairo_context(cr);
    }

    (void)width;
}

void BottomPanel::scrollMoveLogToEnd()
{
    // yalign 1.0 pins the target to the bottom edge so the freshly-appended
    // last line is fully visible, not just scrolled minimally into view. The
    // scroll is to `moveLogEndMark_` (a persistent right-gravity mark), and it
    // is re-issued once on the next idle so it survives GTK's deferred
    // scroll-to-mark pass after the insert relayout — an immediate-only scroll
    // runs against a stale TextView height during a fast burst and lands short.
    if (!moveLogEndMark_)
        return;
    moveLogView_.scroll_to(moveLogEndMark_, 0.0, 0.0, 1.0);
    if (!moveLogScrollIdlePending_) {
        moveLogScrollIdlePending_ = true;
        Glib::signal_idle().connect_once([this] {
            moveLogScrollIdlePending_ = false;
            if (moveLogEndMark_)
                moveLogView_.scroll_to(moveLogEndMark_, 0.0, 0.0, 1.0);
        });
    }
}

sticky_scroll::ScrollGeometry BottomPanel::engineLogGeometry() const
{
    auto adj = scrolledEngineLog_.get_vadjustment();
    if (!adj)
        return {};
    return {adj->get_value(), adj->get_page_size(), adj->get_upper()};
}

void BottomPanel::scrollEngineLogToBottom()
{
    if (!engineLogEndMark_)
        return;

    // Scroll now (handles the case where layout is already current)…
    engineLogView_.scroll_to(engineLogEndMark_, 0.0, 0.0, 1.0);

    // GtkScrolledWindow answers `scroll_to` with a kinetic animation of the
    // vadjustment lasting several frame-clock ticks (tens to ~150ms for a
    // large jump), not an instant set. `programmaticScroll_` is cleared below
    // on the very next idle — ms after this call, long before that animation
    // settles — so the animation's own intermediate frames fire
    // `value_changed` with the guard already down and get mistaken for the
    // user scrolling away, latching `stickToBottom_` off before the view
    // actually reaches bottom (a burst followed within ~50ms by another
    // burst, e.g. a protocol SEND immediately answered by the engine, lands
    // squarely inside that window). Snapping the adjustment's value directly
    // makes the jump immediate and skips the animation (and its transient
    // mid-flight reads) entirely.
    if (auto vadj = scrolledEngineLog_.get_vadjustment()) {
        const double maxValue = vadj->get_upper() - vadj->get_page_size();
        if (maxValue > vadj->get_value())
            vadj->set_value(maxValue);
    }

    // …and once more on the next idle, after GTK has processed the relayout
    // triggered by this tick's insert. During a fast stream the immediate
    // scroll above runs against a stale TextView height and lands short; the
    // deferred re-issue against the SAME persistent mark catches up. This does
    // not re-check "is the user at the bottom" — the decision to stick was
    // already made by the caller before the insert.
    if (!scrollIdlePending_) {
        scrollIdlePending_ = true;
        Glib::signal_idle().connect_once([this] {
            scrollIdlePending_ = false;
            if (engineLogEndMark_)
                engineLogView_.scroll_to(engineLogEndMark_, 0.0, 0.0, 1.0);
            if (auto vadj = scrolledEngineLog_.get_vadjustment()) {
                const double maxValue = vadj->get_upper() - vadj->get_page_size();
                if (maxValue > vadj->get_value())
                    vadj->set_value(maxValue);
            }
            // This tick's programmatic scrolling is done — value_changed may
            // now be trusted as a real user action again.
            programmaticScroll_ = false;
        });
    }
}

bool BottomPanel::flushPending()
{
    if (pendingAppend_.empty())
        return true;  // Nothing to do this tick; keep the timer alive.

    std::vector<EngineLogLine> toApply;
    toApply.swap(pendingAppend_);

    // UI-10: decide stickiness from the remembered intent, falling back to the
    // pre-insert geometry. A single stale "not at bottom" reading on the flush
    // tick must not silently disable auto-scroll (that was the bug).
    const bool wantStick = sticky_scroll::shouldStickToBottom(
        stickToBottom_, engineLogGeometry(), kBottomEpsilon);

    // Everything from here to the trailing scroll is our own doing: the buffer
    // insert, the RT-02 front-trim, and the auto-scroll all move the
    // vadjustment. Suppress the value_changed "did the user scroll away?"
    // check for the duration so a mid-flush transient can't latch stickiness
    // off. Cleared on the scroll's trailing idle (or the plain idle below when
    // we are not sticking this tick).
    programmaticScroll_ = true;

    // Route every queued line through the bounded model first so it is the
    // single source of truth for how many lines are dropped.
    std::size_t totalDropped = 0;
    for (const auto &line : toApply)
        totalDropped += engineLogModel_.push(line);

    auto buf = engineLogView_.get_buffer();
    buf->begin_user_action();

    for (const auto &line : toApply) {
        if (buf->get_char_count() > 0)
            buf->insert(buf->end(), "\n");
        // Payload only — the SEND/MESSAGE/… tag is painted in the gutter
        // (drawGutter), never inserted here, so copying selected rows yields
        // the raw engine text with no prefixes (UI-05).
        buf->insert(buf->end(), logLineClipboardText(line));
    }

    // Trim the buffer to match the model's cap: drop exactly as many lines
    // from the front as the model dropped, so the two never disagree.
    if (totalDropped > 0) {
        auto cut = buf->begin();
        for (std::size_t i = 0; i < totalDropped && cut != buf->end(); ++i)
            cut.forward_line();
        buf->erase(buf->begin(), cut);
    }

    buf->end_user_action();

    if (wantStick) {
        scrollEngineLogToBottom();  // clears programmaticScroll_ on its idle
    } else {
        // Not scrolling this tick — just drop the suppression once the trim's
        // value_changed (if any) has drained.
        Glib::signal_idle().connect_once([this] { programmaticScroll_ = false; });
    }

    gutterArea_.queue_draw();

    return true;
}

void BottomPanel::appendLogLine(const Glib::ustring &prefix, const Glib::ustring &text, LogTagKind tag)
{
    pendingAppend_.push_back(EngineLogLine{std::string(prefix), std::string(text), tag});
}

void BottomPanel::appendSend(const Glib::ustring &text)
{
    appendLogLine("SEND", text, LogTagKind::Send);
}

void BottomPanel::appendRecv(const Glib::ustring &group, const Glib::ustring &text)
{
    LogTagKind tag = LogTagKind::RecvOutput;
    Glib::ustring prefix = group;

    if (group == "Message") {
        tag = LogTagKind::RecvMessage;
    } else if (group == "Debug" || group == "Strat") {
        tag = LogTagKind::RecvMessage;
    } else if (group == "Coord") {
        tag = LogTagKind::RecvCoord;
    } else if (group == "Error") {
        tag = LogTagKind::RecvError;
    } else {
        tag = LogTagKind::RecvOutput;
        prefix = "OUTPUT";
    }

    appendLogLine(prefix.uppercase(), text, tag);
}

void BottomPanel::appendMoveLog(const Glib::ustring &text)
{
    auto buf = moveLogView_.get_buffer();
    buf->insert(buf->end(), text);
    // UI-12: always follow the newest move to the bottom. The "don't yank a
    // user who scrolled up" behaviour (UI-10's remembered-intent + value_changed
    // machinery) is deliberately out of scope here — the Move Log is appended in
    // bursts (a saved game replays every move at once), and a single short-landed
    // scroll mid-burst would latch a naive pre-append at-bottom check permanently
    // off. See docs/todo/UI-12-*.md.
    scrollMoveLogToEnd();
}

void BottomPanel::clear()
{
    moveLogView_.get_buffer()->set_text("");
}

void BottomPanel::clearEngineLog()
{
    pendingAppend_.clear();
    engineLogModel_.clear();
    engineLogView_.get_buffer()->set_text("");
    // UI-10: an empty log is trivially "at the bottom" — resume stickiness so
    // the next analysis run follows its output from the first line.
    stickToBottom_      = true;
    programmaticScroll_ = false;
    gutterArea_.queue_draw();
}
