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
    // Single TextView: direction label ([SEND]/[RECV]) is inserted inline as
    // a tagged (colored) prefix in the same buffer as the content, so the
    // whole line — label plus content — wraps and scrolls as one unit. This
    // replaces the previous dual-TextView gutter+content split, whose
    // mismatched wrap modes (NONE vs WORD_CHAR) let the label desync from its
    // line once a long RECV line wrapped (RT-02).
    engineLogView_.set_editable(false);
    engineLogView_.set_cursor_visible(false);
    engineLogView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    engineLogView_.add_css_class("monospace");

    // Create text tags for SEND/RECV colors (scheme unchanged from before).
    auto buf = engineLogView_.get_buffer();
    auto tags = buf->get_tag_table();

    tagSend_ = Gtk::TextTag::create("send");
    tagSend_->property_foreground() = "#6ab0f3";  // Blue
    tagSend_->property_weight() = 700;
    tags->add(tagSend_);

    tagRecvOutput_ = Gtk::TextTag::create("recv_output");
    tagRecvOutput_->property_foreground() = "#8cc265";  // Green
    tagRecvOutput_->property_weight() = 700;
    tags->add(tagRecvOutput_);

    tagRecvMessage_ = Gtk::TextTag::create("recv_message");
    tagRecvMessage_->property_foreground() = "#c678dd";  // Purple
    tagRecvMessage_->property_weight() = 700;
    tags->add(tagRecvMessage_);

    tagRecvCoord_ = Gtk::TextTag::create("recv_coord");
    tagRecvCoord_->property_foreground() = "#e5c07b";  // Yellow/Orange
    tagRecvCoord_->property_weight() = 700;
    tags->add(tagRecvCoord_);

    tagRecvError_ = Gtk::TextTag::create("recv_error");
    tagRecvError_->property_foreground() = "#e06c75";  // Red
    tagRecvError_->property_weight() = 700;
    tags->add(tagRecvError_);

    scrolledEngineLog_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    engineLogOverlay_.setContent(engineLogView_);
    engineLogOverlay_.setEmpty(true);  // Empty buffer at construction.
    scrolledEngineLog_.set_child(engineLogOverlay_);
    scrolledEngineLog_.set_hexpand(true);
    scrolledEngineLog_.set_vexpand(true);

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

    engineLogBox_.append(scrolledEngineLog_);
    engineLogBox_.append(commandEntry_);
    append_page(engineLogBox_, "Engine Log");

    set_size_request(-1, 120);
}

Glib::RefPtr<Gtk::TextTag> BottomPanel::tagForKind(LogTagKind tag) const
{
    switch (tag) {
        case LogTagKind::Send:        return tagSend_;
        case LogTagKind::RecvMessage: return tagRecvMessage_;
        case LogTagKind::RecvCoord:   return tagRecvCoord_;
        case LogTagKind::RecvError:   return tagRecvError_;
        case LogTagKind::RecvOutput:
        default:                     return tagRecvOutput_;
    }
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
        auto end = buf->end();
        if (buf->get_char_count() > 0)
            buf->insert(end, "\n");
        end = buf->end();
        buf->insert_with_tag(end, line.prefix, tagForKind(line.tag));
        end = buf->end();
        buf->insert(end, " ");
        end = buf->end();
        buf->insert(end, line.text);
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
}
