#pragma once

#include "command/palette_search.h"

#include <gtkmm.h>
#include <sigc++/sigc++.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

/// PAL-02: one runnable palette row. `entry` is what the search engine ranks;
/// `run` is invoked after the palette has closed and focus was restored.
struct PaletteItem {
    palette_search::Entry entry;
    std::function<void()> run;
    std::string           shortcut;  ///< Optional hint shown at the row's right edge.
};

/// Ctrl+K command palette (features/command-palette). A popover anchored to the
/// top-centre of `anchor` holding a search entry and a ranked result list.
/// Ranking lives in palette_search (PAL-01); this class is only the front end.
/// Items are re-queried from the provider on every open, so extension commands
/// and enabled-state changes are picked up without caching.
class CommandPalette {
public:
    CommandPalette(Gtk::Widget &anchor, const std::string &lexiconTsv);
    ~CommandPalette();

    /// Supplies the items each time the palette opens.
    void setItemsProvider(std::function<std::vector<PaletteItem>()> provider);
    /// Most-recently-run ids, newest first (empty query shows these; ranking boost).
    void setRecentProvider(std::function<std::vector<std::string>()> provider);

    /// Emitted with the item id after a row ran (MainWindow records it as recent).
    sigc::signal<void(std::string)> signal_item_run;

    void open();

    /// I18N-02: re-apply placeholder / chip text after a language change.
    void retranslate();
    void close();
    bool isOpen() const { return popover_.is_visible(); }

    // ── test seams ──
    void                     setQuery(const std::string &text);
    std::vector<std::string> resultIds() const;
    std::string              scopeText() const;   ///< Text of the active-scope chip.
    std::string              emptyStateText() const;  ///< Text shown when no row matches.
    bool                     runSelected();       ///< Same path as Enter.
    void                     moveSelection(int delta);  ///< Same path as Up/Down.

private:
    void refresh();
    void rebuildRows(const std::vector<std::size_t> &itemIndexes);
    bool onKey(guint keyval);
    void runRow(Gtk::ListBoxRow *row);

    Gtk::Widget &anchor_;
    Gtk::Popover popover_;
    Gtk::Box     box_{Gtk::Orientation::VERTICAL, 6};
    Gtk::Box         entryRow_{Gtk::Orientation::HORIZONTAL, 6};
    Gtk::SearchEntry entry_;
    Gtk::Label       scopeChip_;  ///< PAL-04: shows which kinds the query searches.
    Gtk::ScrolledWindow scroller_;
    Gtk::ListBox list_;

    std::string                    lexiconTsv_;
    std::function<std::vector<PaletteItem>()>  itemsProvider_;
    std::function<std::vector<std::string>()>  recentProvider_;
    std::vector<PaletteItem>       items_;
    std::vector<std::string>       recent_;
    std::unique_ptr<palette_search::Index> index_;
    std::vector<std::size_t>       rowItem_;  ///< Row position -> items_ index.
    GtkWidget                     *prevFocus_ = nullptr;  ///< Weak pointer, cleared on destroy.
    bool                           suppress_ = false;
};
