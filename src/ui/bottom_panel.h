#pragma once

#include "engine_log_model.h"
#include "sticky_scroll.h"
#include <gtkmm.h>
#include <sigc++/sigc++.h>
#include <functional>
#include <string>
#include <vector>

struct RanlsCons01Probe;  // tests/test_cons01_command_autocomplete.cpp
struct RanlsCons02Probe;  // tests/test_cons02_autocorrect_dispatch.cpp

/// Tabbed bottom panel with Move Log, Engine Log + command input.
///
/// Engine Log uses a single TextView holding ONLY the raw engine payload
/// (one line per logical engine line), plus a sibling fixed-width
/// `Gtk::DrawingArea` gutter that paints the direction/category tag
/// (SEND/MESSAGE/OUTPUT/…) beside each line — like a text editor's
/// line-number gutter (UI-05). The gutter is a drawn widget with no text
/// nodes, so selecting log rows and copying yields the payload only, no
/// prefixes. It paints each tag at the live y-position of its buffer line
/// (`get_line_yrange` minus the scroll offset), so it can never desync from
/// its line on wrap or resize (see RT-02). Content is bounded to
/// `EngineLogModel`'s line cap (which is also the source of truth for each
/// line's tag) and appends are batched on a timer tick rather than one buffer
/// transaction per line (also RT-02).
class BottomPanel : public Gtk::Notebook {
public:
    BottomPanel();
    ~BottomPanel() override;

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

    /// CONS-01: supplier of the current `!` command-name set for AutoComplete.
    /// MainWindow wires this to CommandDispatcher::registeredNames(). Queried
    /// fresh on every suggestion refresh — never cached here (HC4).
    void setCommandNameProvider(std::function<std::vector<std::string>()> provider);

    /// CONS-01: supplier of the `usage` string for a completed command name
    /// (CommandDispatcher::commandUsage). Shown as the static argument hint.
    void setCommandUsageProvider(std::function<std::string(const std::string &)> provider);

private:
    friend struct ::RanlsCons01Probe;
    friend struct ::RanlsCons02Probe;

    // ── CONS-01: command-entry AutoComplete ─────────────────────────────────
    /// Recompute the candidate set for the current entry text + caret and
    /// show/hide the popover and ghost-text. Called on every `notify::text`
    /// (guarded by `suppressSuggest_` so our own programmatic `set_text`
    /// during Tab-complete / history nav does not re-enter).
    void refreshSuggestions();
    /// Handle a key while the suggestion popover is logically open. Returns
    /// true iff the key was consumed (Tab/Up/Down/Enter/Esc). Any other key
    /// falls through to normal entry editing.
    bool handleSuggestionKey(unsigned int keyval);
    void hideSuggestions();
    void rebuildSuggestionRows();
    void updateGhostText();
    /// Tab: complete the first token to the longest common prefix; if already
    /// at the LCP, cycle to the next candidate.
    void completeOrCycle();
    /// Enter (popover open): drop the highlighted candidate into the entry
    /// with a trailing space. Does NOT submit (R3).
    void acceptSelectedSuggestion();
    /// Rewrite just the `!<name>` first token of the entry text, keeping any
    /// leading whitespace and everything after the first token. Guarded so it
    /// does not trigger a suggestion refresh from inside itself.
    void applyFirstToken(const std::string &name, bool trailingSpace);
    void moveSuggestionSelection(int delta);

    std::function<std::vector<std::string>()>              commandNameProvider_;
    std::function<std::string(const std::string &)>        commandUsageProvider_;

    Gtk::Overlay commandOverlay_;
    Gtk::Label   ghostLabel_;
    Gtk::Popover suggestionPopover_;
    Gtk::ListBox suggestionList_;

    std::vector<std::string> suggestionMatches_;
    int  suggestionSel_   = -1;     ///< highlighted row index into suggestionMatches_
    int  tabCycle_        = -1;     ///< last index applied by a Tab cycle
    bool suggestOpen_     = false;  ///< popover is logically open (drives key routing)
    bool suppressSuggest_ = false;  ///< re-entrancy guard around programmatic set_text

    void appendLogLine(const Glib::ustring &prefix, const Glib::ustring &text, LogTagKind tag);

    /// CSS-independent color for a tag kind, used to paint the gutter labels.
    /// Mirrors the historical SEND/RECV color scheme (see RT-02 scope note:
    /// the scheme itself must not change).
    static const char *gutterColorForKind(LogTagKind tag);

    /// Draw callback for `gutterArea_`: paints each visible buffer line's
    /// direction tag at that line's current y-position. Positions come from
    /// the live TextView layout, so wrapping/resize can't desync them.
    void drawGutter(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);

    /// Flushes all pending (batched) lines into the GTK buffer in a single
    /// transaction, enforces the EngineLogModel line cap on the buffer, and
    /// auto-scrolls only if the view was already at the bottom. Returns true
    /// so it can be used directly as a Glib::signal_timeout callback.
    bool flushPending();

    /// Current vertical scroll geometry of the Engine Log's ScrolledWindow.
    sticky_scroll::ScrollGeometry engineLogGeometry() const;

    /// Scroll the Engine Log to its end via a persistent end-of-buffer mark,
    /// re-issued once on the next idle so it survives GTK's deferred-scroll
    /// pass after the batch relayout (UI-10). Carries the caller's already-made
    /// "stick to bottom" decision — never re-evaluates at-bottom itself.
    void scrollEngineLogToBottom();

    /// UI-12: scroll the Move Log to its end via a persistent end-of-buffer
    /// mark, re-issued once on the next idle so it survives GTK's deferred
    /// scroll-to-mark pass after the insert relayout.
    void scrollMoveLogToEnd();

    /// UI-10: pixel tolerance for "at the bottom".
    static constexpr double kBottomEpsilon = 1.0;

    /// UI-10: persisted "the user wants the Engine Log pinned to the newest
    /// line" intent. Starts true; cleared only when the user scrolls up
    /// (see the vadjustment value_changed handler); restored when they scroll
    /// back to the bottom. A stale mid-stream geometry read never clears it.
    bool stickToBottom_ = true;

    /// UI-10: guards against queueing more than one idle re-scroll at a time.
    bool scrollIdlePending_ = false;

    /// UI-12: same one-in-flight guard for the Move Log's idle re-scroll.
    bool moveLogScrollIdlePending_ = false;

    /// UI-12: right-gravity mark permanently anchored at the end of the Move
    /// Log buffer, created once — see `scrollMoveLogToEnd`.
    Glib::RefPtr<Gtk::TextBuffer::Mark> moveLogEndMark_;

    /// UI-10: set while `flushPending` is mutating the buffer and auto-scrolling
    /// so the vadjustment `value_changed` handler does not mistake our own
    /// scroll / RT-02 front-trim for the user scrolling away from the bottom.
    /// Cleared on the trailing idle of that flush.
    bool programmaticScroll_ = false;

    /// UI-10: right-gravity mark permanently anchored at the end of the engine
    /// log buffer, created once. Scrolling to a stable mark (instead of
    /// create-scroll-delete each tick) lets GTK's deferred scroll still find
    /// its target after the post-insert relayout.
    Glib::RefPtr<Gtk::TextBuffer::Mark> engineLogEndMark_;

    Gtk::ScrolledWindow scrolledMoveLog_;
    Gtk::TextView       moveLogView_;

    // Engine log — payload-only TextView + sibling drawn gutter (UI-05).
    Gtk::Box            engineLogRow_{Gtk::Orientation::HORIZONTAL};
    Gtk::DrawingArea    gutterArea_;
    Gtk::ScrolledWindow scrolledEngineLog_;
    Gtk::TextView       engineLogView_;
    int                 gutterWidth_ = 0;  // computed lazily on first draw

    // UI-08 removed the idle-state placeholder text; UI-10 (Engine Log) and
    // UI-12 (Move Log) then dropped the `EmptyStateOverlay` wrappers entirely —
    // a `Gtk::Overlay` between a `Gtk::ScrolledWindow` and its `Gtk::TextView`
    // is not `Gtk::Scrollable`, so it forced an implicit `Gtk::Viewport` that
    // silently swallowed every `TextView::scroll_to`. Both TextViews are now
    // their ScrolledWindow's direct child.

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
