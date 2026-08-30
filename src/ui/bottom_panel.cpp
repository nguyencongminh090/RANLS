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
    moveLogOverlay_.setContent(moveLogView_);
    moveLogOverlay_.setEmpty(true);  // Empty buffer at construction.
    scrolledMoveLog_.set_child(moveLogOverlay_);
    append_page(scrolledMoveLog_, "Move Log");

    // ── Engine Log tab ──────────────────────────────────────────────────────
    // UI-05: the TextView holds ONLY the raw engine payload (one line per
    // logical engine line). The direction/category tag ([SEND]/[MESSAGE]/…)
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

    gutterArea_.add_css_class("engine-gutter");
    gutterArea_.set_valign(Gtk::Align::FILL);
    gutterArea_.set_draw_func(sigc::mem_fun(*this, &BottomPanel::drawGutter));

    scrolledEngineLog_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    engineLogOverlay_.setContent(engineLogView_);
    engineLogOverlay_.setEmpty(true);  // Empty buffer at construction.
    scrolledEngineLog_.set_child(engineLogOverlay_);
    scrolledEngineLog_.set_hexpand(true);
    scrolledEngineLog_.set_vexpand(true);

    // Repaint the gutter whenever the log scrolls or relayouts (resize/wrap
    // change the adjustment's range even when its value is unchanged).
    if (auto vadj = scrolledEngineLog_.get_vadjustment()) {
        vadj->signal_value_changed().connect([this] { gutterArea_.queue_draw(); });
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
    const Pango::FontDescription fontDesc("Monospace 11");

    // Fixed column width: wide enough for the longest tag, computed once.
    if (gutterWidth_ == 0) {
        auto probe = gutterArea_.create_pango_layout("[MESSAGE]");
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

void BottomPanel::updateMoveLogEmptyState()
{
    moveLogOverlay_.setEmpty(moveLogView_.get_buffer()->get_char_count() == 0);
}

void BottomPanel::updateEngineLogEmptyState()
{
    engineLogOverlay_.setEmpty(engineLogView_.get_buffer()->get_char_count() == 0);
}

bool BottomPanel::isScrolledToBottom()
{
    auto adj = scrolledEngineLog_.get_vadjustment();
    if (!adj) return true;
    constexpr double kEpsilon = 1.0;  // pixel tolerance
    return adj->get_value() + adj->get_page_size() >= adj->get_upper() - kEpsilon;
}

bool BottomPanel::flushPending()
{
    if (pendingAppend_.empty())
        return true;  // Nothing to do this tick; keep the timer alive.

    std::vector<EngineLogLine> toApply;
    toApply.swap(pendingAppend_);

    bool wasAtBottom = isScrolledToBottom();

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
        // Payload only — the [SEND]/[MESSAGE]/… tag is painted in the gutter
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

    if (wasAtBottom) {
        auto mark = buf->create_mark(buf->end());
        engineLogView_.scroll_to(mark);
        buf->delete_mark(mark);
    }

    updateEngineLogEmptyState();
    gutterArea_.queue_draw();

    return true;
}

void BottomPanel::appendLogLine(const Glib::ustring &prefix, const Glib::ustring &text, LogTagKind tag)
{
    pendingAppend_.push_back(EngineLogLine{std::string(prefix), std::string(text), tag});
}

void BottomPanel::appendSend(const Glib::ustring &text)
{
    appendLogLine("[SEND]", text, LogTagKind::Send);
}

void BottomPanel::appendRecv(const Glib::ustring &group, const Glib::ustring &text)
{
    LogTagKind tag = LogTagKind::RecvOutput;
    Glib::ustring prefix = "[" + group + "]";

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
        prefix = "[OUTPUT]";
    }

    appendLogLine(prefix.uppercase(), text, tag);
}

void BottomPanel::appendMoveLog(const Glib::ustring &text)
{
    auto buf = moveLogView_.get_buffer();
    buf->insert(buf->end(), text);
    updateMoveLogEmptyState();
}

void BottomPanel::clear()
{
    moveLogView_.get_buffer()->set_text("");
    updateMoveLogEmptyState();
}

void BottomPanel::clearEngineLog()
{
    pendingAppend_.clear();
    engineLogModel_.clear();
    engineLogView_.get_buffer()->set_text("");
    updateEngineLogEmptyState();
    gutterArea_.queue_draw();
}
