#pragma once

#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <string>
#include <vector>

/// Tabbed bottom panel with Move Log, Engine Log + command input.
/// Engine Log uses a dual-TextView gutter pattern: a non-selectable direction
/// label column ([SEND]/[RECV]) and a selectable content column.
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
    void appendLogLine(const Glib::ustring &prefix, const Glib::ustring &text, Glib::RefPtr<Gtk::TextTag> tag);
    void syncScroll();

    Gtk::ScrolledWindow scrolledMoveLog_;
    Gtk::TextView       moveLogView_;

    // Engine log — dual TextView gutter pattern.
    Gtk::ScrolledWindow scrolledEngineContent_;  ///< Scrolled window for content.
    Gtk::ScrolledWindow scrolledEngineGutter_;   ///< Scrolled window for gutter (synced).
    Gtk::TextView       engineGutterView_;       ///< Non-selectable direction labels.
    Gtk::TextView       engineContentView_;      ///< Selectable log content.

    Glib::RefPtr<Gtk::TextTag> tagSend_;
    Glib::RefPtr<Gtk::TextTag> tagRecvOutput_;
    Glib::RefPtr<Gtk::TextTag> tagRecvMessage_;
    Glib::RefPtr<Gtk::TextTag> tagRecvCoord_;
    Glib::RefPtr<Gtk::TextTag> tagRecvError_;

    Gtk::Box            engineLogBox_{Gtk::Orientation::VERTICAL};
    Gtk::Box            engineLogViews_{Gtk::Orientation::HORIZONTAL};
    Gtk::Entry          commandEntry_;
    std::vector<std::string> commandHistory_;
    int                      historyIdx_ = -1;
    bool                     syncingScroll_ = false;
};
