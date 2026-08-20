#include "bottom_panel.h"

BottomPanel::BottomPanel()
{
    add_css_class("bottom-panel");

    // ── Move Log tab ────────────────────────────────────────────────────────
    moveLogView_.set_editable(false);
    moveLogView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    scrolledMoveLog_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledMoveLog_.set_child(moveLogView_);
    append_page(scrolledMoveLog_, "Move Log");

    // ── Engine Log tab ──────────────────────────────────────────────────────
    // Gutter (direction labels) — non-selectable, non-focusable.
    engineGutterView_.set_editable(false);
    engineGutterView_.set_cursor_visible(false);
    engineGutterView_.set_can_focus(false);         // Cannot receive focus.
    engineGutterView_.set_focusable(false);
    engineGutterView_.set_wrap_mode(Gtk::WrapMode::NONE);
    engineGutterView_.add_css_class("monospace");
    engineGutterView_.add_css_class("engine-gutter");

    // Create text tags for SEND/RECV colors in the gutter.
    auto gutterBuf = engineGutterView_.get_buffer();
    auto gutterTags = gutterBuf->get_tag_table();

    tagSend_ = Gtk::TextTag::create("send");
    tagSend_->property_foreground() = "#6ab0f3";  // Blue
    tagSend_->property_weight() = 700;
    gutterTags->add(tagSend_);

    tagRecvOutput_ = Gtk::TextTag::create("recv_output");
    tagRecvOutput_->property_foreground() = "#8cc265";  // Green
    tagRecvOutput_->property_weight() = 700;
    gutterTags->add(tagRecvOutput_);

    tagRecvMessage_ = Gtk::TextTag::create("recv_message");
    tagRecvMessage_->property_foreground() = "#c678dd";  // Purple
    tagRecvMessage_->property_weight() = 700;
    gutterTags->add(tagRecvMessage_);

    tagRecvCoord_ = Gtk::TextTag::create("recv_coord");
    tagRecvCoord_->property_foreground() = "#e5c07b";  // Yellow/Orange
    tagRecvCoord_->property_weight() = 700;
    gutterTags->add(tagRecvCoord_);

    tagRecvError_ = Gtk::TextTag::create("recv_error");
    tagRecvError_->property_foreground() = "#e06c75";  // Red
    tagRecvError_->property_weight() = 700;
    gutterTags->add(tagRecvError_);

    scrolledEngineGutter_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::EXTERNAL);
    scrolledEngineGutter_.set_child(engineGutterView_);
    scrolledEngineGutter_.set_size_request(56, -1);  // Fixed width for "[SEND]"/"[RECV]".

    // Content — selectable.
    engineContentView_.set_editable(false);
    engineContentView_.set_cursor_visible(false);
    engineContentView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    engineContentView_.add_css_class("monospace");

    scrolledEngineContent_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledEngineContent_.set_child(engineContentView_);
    scrolledEngineContent_.set_hexpand(true);
    scrolledEngineContent_.set_vexpand(true);

    // Sync gutter scroll with content scroll.
    auto vAdj = scrolledEngineContent_.get_vadjustment();
    vAdj->signal_value_changed().connect(sigc::mem_fun(*this, &BottomPanel::syncScroll));

    // Lay out gutter + content side by side.
    engineLogViews_.append(scrolledEngineGutter_);
    engineLogViews_.append(scrolledEngineContent_);
    engineLogViews_.set_vexpand(true);

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

    engineLogBox_.append(engineLogViews_);
    engineLogBox_.append(commandEntry_);
    append_page(engineLogBox_, "Engine Log");

    set_size_request(-1, 120);
}

void BottomPanel::syncScroll()
{
    if (syncingScroll_) return;
    syncingScroll_ = true;
    auto contentAdj = scrolledEngineContent_.get_vadjustment();
    auto gutterAdj  = scrolledEngineGutter_.get_vadjustment();
    gutterAdj->set_value(contentAdj->get_value());
    syncingScroll_ = false;
}

void BottomPanel::appendLogLine(const Glib::ustring &prefix, const Glib::ustring &text, Glib::RefPtr<Gtk::TextTag> tag)
{
    // Append direction label to gutter.
    auto gutterBuf = engineGutterView_.get_buffer();
    auto gutterEnd = gutterBuf->end();

    // Add newline before if not first line.
    if (gutterBuf->get_char_count() > 0)
        gutterBuf->insert(gutterEnd, "\n");
    gutterEnd = gutterBuf->end();
    gutterBuf->insert_with_tag(gutterEnd, prefix, tag);

    // Append content text.
    auto contentBuf = engineContentView_.get_buffer();
    auto contentEnd = contentBuf->end();
    if (contentBuf->get_char_count() > 0)
        contentBuf->insert(contentEnd, "\n");
    contentEnd = contentBuf->end();
    contentBuf->insert(contentEnd, text);

    // Auto-scroll content to bottom (gutter follows via sync).
    auto mark = contentBuf->create_mark(contentBuf->end());
    engineContentView_.scroll_to(mark);
    contentBuf->delete_mark(mark);
}

void BottomPanel::appendSend(const Glib::ustring &text)
{
    appendLogLine("[SEND]", text, tagSend_);
}

void BottomPanel::appendRecv(const Glib::ustring &group, const Glib::ustring &text)
{
    Glib::RefPtr<Gtk::TextTag> tag = tagRecvOutput_;
    Glib::ustring prefix = "[" + group + "]";

    if (group == "Message") {
        tag = tagRecvMessage_;
    } else if (group == "Debug" || group == "Strat") {
        tag = tagRecvMessage_;
    } else if (group == "Coord") {
        tag = tagRecvCoord_;
    } else if (group == "Error") {
        tag = tagRecvError_;
    } else {
        tag = tagRecvOutput_;
        prefix = "[OUTPUT]";
    }

    appendLogLine(prefix.uppercase(), text, tag);
}

void BottomPanel::appendMoveLog(const Glib::ustring &text)
{
    auto buf = moveLogView_.get_buffer();
    buf->insert(buf->end(), text);
}

void BottomPanel::clear()
{
    moveLogView_.get_buffer()->set_text("");
}

void BottomPanel::clearEngineLog()
{
    engineGutterView_.get_buffer()->set_text("");
    engineContentView_.get_buffer()->set_text("");
}
