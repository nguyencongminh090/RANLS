#pragma once

#include "engine_log_model.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

/// Tabbed bottom panel with Move Log, Engine Log + command input.
///
/// Engine Log uses a single TextView with a tagged (colored) prefix column
/// inline in the same text flow as the line content — not a dual-TextView
/// gutter+content split. A single buffer means the direction label
/// ([SEND]/[RECV]) always wraps together with its own line, so there is no
/// gutter/content desync to keep in sync (see RT-02). Content is bounded to
/// `EngineLogModel`'s line cap and appends are batched on a timer tick rather
/// than one buffer transaction per line (also RT-02).
class BottomPanel : public Gtk::Notebook {
public:
    BottomPanel();

    /// Append a SEND (stdin) line to the engine log.
    void appendSend(const Glib::ustring &text);

    /// Append a RECV (stdout) line to the engine log.
    void appendRecv(const Glib::ustring &group, const Glib::ustring &text);

    void appendMoveLog(const Glib::ustring &text);
    void clear();

    /// Clear only the engine log (gutter + content), keeping move log intact.
    void clearEngineLog();

    /// Signal emitted when user submits a command in the engine log entry.
    sigc::signal<void(std::string)> signal_command_sent;

private:
    void appendLogLine(const Glib::ustring &prefix, const Glib::ustring &text, LogTagKind tag);
    Glib::RefPtr<Gtk::TextTag> tagForKind(LogTagKind tag) const;

    /// Flushes all pending (batched) lines into the GTK buffer in a single
    /// transaction, enforces the EngineLogModel line cap on the buffer, and
    /// auto-scrolls only if the view was already at the bottom. Returns true
    /// so it can be used directly as a Glib::signal_timeout callback.
    bool flushPending();

    /// Whether the engine log view's vertical scroll is currently at (or
    /// within a small tolerance of) the bottom.
    bool isScrolledToBottom();

    Gtk::ScrolledWindow scrolledMoveLog_;
    Gtk::TextView       moveLogView_;

    // Engine log — single TextView, tagged prefix inline with content.
    Gtk::ScrolledWindow scrolledEngineLog_;
    Gtk::TextView       engineLogView_;

    Glib::RefPtr<Gtk::TextTag> tagSend_;
    Glib::RefPtr<Gtk::TextTag> tagRecvOutput_;
    Glib::RefPtr<Gtk::TextTag> tagRecvMessage_;
    Glib::RefPtr<Gtk::TextTag> tagRecvCoord_;
    Glib::RefPtr<Gtk::TextTag> tagRecvError_;

    // Bounded, GUI-independent model of the retained log lines (source of
    // truth for the line cap) and the batch queue awaiting the next flush.
    EngineLogModel               engineLogModel_{5000};
    std::vector<EngineLogLine>   pendingAppend_;
    sigc::connection              flushTimerConn_;

    Gtk::Box            engineLogBox_{Gtk::Orientation::VERTICAL};
    Gtk::Entry          commandEntry_;
    std::vector<std::string> commandHistory_;
    int                      historyIdx_ = -1;
};
