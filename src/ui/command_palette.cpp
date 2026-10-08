#include "ui/command_palette.h"

#include "command/palette_recent.h"

#include <algorithm>

namespace {
constexpr int kMaxRows = 8;
}

CommandPalette::CommandPalette(Gtk::Widget &anchor, const std::string &lexiconTsv)
    : anchor_(anchor), lexiconTsv_(lexiconTsv)
{
    popover_.set_parent(anchor_);
    popover_.set_has_arrow(false);
    popover_.set_autohide(true);
    popover_.set_position(Gtk::PositionType::BOTTOM);
    popover_.add_css_class("palette-popover");

    entry_.set_placeholder_text("Search actions, settings and ! commands…");
    entry_.set_hexpand(true);

    list_.set_selection_mode(Gtk::SelectionMode::BROWSE);
    list_.set_activate_on_single_click(true);
    list_.add_css_class("palette-list");
    scroller_.set_child(list_);
    scroller_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::AUTOMATIC);
    scroller_.set_min_content_height(40);
    scroller_.set_max_content_height(360);
    scroller_.set_propagate_natural_height(true);

    box_.set_margin(8);
    box_.set_size_request(560, -1);
    box_.append(entry_);
    box_.append(scroller_);
    popover_.set_child(box_);

    entry_.signal_changed().connect([this]() {
        if (!suppress_)
            refresh();
    });
    list_.signal_row_activated().connect([this](Gtk::ListBoxRow *row) { runRow(row); });

    // One key controller on the entry (capture phase so Up/Down reach us before
    // the text widget moves the caret). Returns true only for keys it consumes.
    auto keys = Gtk::EventControllerKey::create();
    keys->set_propagation_phase(Gtk::PropagationPhase::CAPTURE);
    keys->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) { return onKey(keyval); }, false);
    entry_.add_controller(keys);
    entry_.signal_activate().connect([this]() { runSelected(); });

    popover_.signal_closed().connect([this]() {
        // Restore focus to whatever had it before open() (if it still exists).
        if (prevFocus_) {
            GtkWidget *w = prevFocus_;
            g_object_remove_weak_pointer(G_OBJECT(w), reinterpret_cast<gpointer *>(&prevFocus_));
            prevFocus_ = nullptr;
            gtk_widget_grab_focus(w);
        }
    });
}

CommandPalette::~CommandPalette()
{
    if (prevFocus_)
        g_object_remove_weak_pointer(G_OBJECT(prevFocus_), reinterpret_cast<gpointer *>(&prevFocus_));
    popover_.unparent();
}

void CommandPalette::setItemsProvider(std::function<std::vector<PaletteItem>()> provider)
{
    itemsProvider_ = std::move(provider);
}

void CommandPalette::setRecentProvider(std::function<std::vector<std::string>()> provider)
{
    recentProvider_ = std::move(provider);
}

void CommandPalette::open()
{
    items_  = itemsProvider_ ? itemsProvider_() : std::vector<PaletteItem>{};
    recent_ = recentProvider_ ? recentProvider_() : std::vector<std::string>{};
    std::vector<palette_search::Entry> entries;
    entries.reserve(items_.size());
    for (const auto &it : items_)
        entries.push_back(it.entry);
    index_ = std::make_unique<palette_search::Index>(std::move(entries), palette_search::Lexicon::parse(lexiconTsv_));

    if (auto *root = anchor_.get_root()) {
        if (GtkWidget *f = gtk_window_get_focus(GTK_WINDOW(root->gobj()))) {
            if (prevFocus_)
                g_object_remove_weak_pointer(G_OBJECT(prevFocus_), reinterpret_cast<gpointer *>(&prevFocus_));
            prevFocus_ = f;
            g_object_add_weak_pointer(G_OBJECT(f), reinterpret_cast<gpointer *>(&prevFocus_));
        }
    }

    const int w = anchor_.get_width();
    popover_.set_pointing_to(Gdk::Rectangle(w / 2, 56, 1, 1));

    suppress_ = true;
    entry_.set_text("");
    suppress_ = false;
    refresh();
    popover_.popup();
    entry_.grab_focus();
}

void CommandPalette::close()
{
    popover_.popdown();
}

void CommandPalette::setQuery(const std::string &text)
{
    entry_.set_text(text);  // fires signal_changed -> refresh()
    if (text.empty())
        refresh();
}

std::vector<std::string> CommandPalette::resultIds() const
{
    std::vector<std::string> ids;
    for (std::size_t i : rowItem_)
        ids.push_back(items_[i].entry.id);
    return ids;
}

void CommandPalette::refresh()
{
    std::vector<std::size_t> shown;
    if (!index_) {
        rebuildRows(shown);
        return;
    }
    const std::string q = entry_.get_text();
    if (q.find_first_not_of(" \t") == std::string::npos) {
        // Empty query: the recently run items, newest first.
        for (const auto &id : recent_)
            for (std::size_t i = 0; i < items_.size(); ++i)
                if (items_[i].entry.id == id && shown.size() < kMaxRows) {
                    shown.push_back(i);
                    break;
                }
    } else {
        auto hits = index_->search(q, kMaxRows);
        palette_recent::applyRecency(hits, *index_, recent_);
        for (const auto &h : hits)
            shown.push_back(h.index);
    }
    rebuildRows(shown);
}

void CommandPalette::rebuildRows(const std::vector<std::size_t> &itemIndexes)
{
    while (auto *child = list_.get_first_child())
        list_.remove(*child);
    rowItem_ = itemIndexes;

    if (itemIndexes.empty()) {
        const bool empty = entry_.get_text().find_first_not_of(" \t") == std::string::npos;
        auto *lbl = Gtk::make_managed<Gtk::Label>(empty ? "Type to search — try \"settings\", \"cài đặt\" or \"!undo\""
                                                        : "No matches");
        lbl->add_css_class("dim-label");
        lbl->set_margin(10);
        auto *row = Gtk::make_managed<Gtk::ListBoxRow>();
        row->set_child(*lbl);
        row->set_selectable(false);
        row->set_activatable(false);
        list_.append(*row);
        return;
    }

    for (std::size_t idx : itemIndexes) {
        const PaletteItem &it = items_[idx];
        auto *row  = Gtk::make_managed<Gtk::ListBoxRow>();
        auto *hbox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 10);
        hbox->set_margin(6);

        auto *texts = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 0);
        texts->set_hexpand(true);
        auto *title = Gtk::make_managed<Gtk::Label>(it.entry.titleEn);
        title->set_halign(Gtk::Align::START);
        title->set_xalign(0.f);
        title->set_ellipsize(Pango::EllipsizeMode::END);
        texts->append(*title);
        if (!it.entry.titleVi.empty() && it.entry.titleVi != it.entry.titleEn) {
            auto *vi = Gtk::make_managed<Gtk::Label>(it.entry.titleVi);
            vi->set_halign(Gtk::Align::START);
            vi->set_xalign(0.f);
            vi->add_css_class("dim-label");
            vi->set_ellipsize(Pango::EllipsizeMode::END);
            texts->append(*vi);
        }
        hbox->append(*texts);

        if (!it.shortcut.empty()) {
            auto *sc = Gtk::make_managed<Gtk::Label>(it.shortcut);
            sc->add_css_class("dim-label");
            sc->set_valign(Gtk::Align::CENTER);
            hbox->append(*sc);
        }
        auto *kind = Gtk::make_managed<Gtk::Label>(it.entry.group);
        kind->add_css_class("dim-label");
        kind->add_css_class("palette-kind");
        kind->set_valign(Gtk::Align::CENTER);
        hbox->append(*kind);

        row->set_child(*hbox);
        row->set_sensitive(it.entry.enabled);
        list_.append(*row);
    }
    if (auto *first = list_.get_row_at_index(0))
        list_.select_row(*first);
}

void CommandPalette::moveSelection(int delta)
{
    if (rowItem_.empty())
        return;
    auto *cur = list_.get_selected_row();
    int   idx = cur ? cur->get_index() : -1;
    idx = std::clamp(idx + delta, 0, static_cast<int>(rowItem_.size()) - 1);
    if (auto *row = list_.get_row_at_index(idx))
        list_.select_row(*row);
}

bool CommandPalette::onKey(guint keyval)
{
    switch (keyval) {
    case GDK_KEY_Down:
        moveSelection(+1);
        return true;
    case GDK_KEY_Up:
        moveSelection(-1);
        return true;
    case GDK_KEY_Escape:
        close();
        return true;
    default:
        return false;
    }
}

bool CommandPalette::runSelected()
{
    if (auto *row = list_.get_selected_row()) {
        runRow(row);
        return true;
    }
    return false;
}

void CommandPalette::runRow(Gtk::ListBoxRow *row)
{
    if (!row)
        return;
    const int idx = row->get_index();
    if (idx < 0 || idx >= static_cast<int>(rowItem_.size()))
        return;
    const PaletteItem item = items_[rowItem_[static_cast<std::size_t>(idx)]];  // copy: close() may rebuild
    if (!item.entry.enabled)
        return;
    close();  // the popover's closed handler restores the previous focus...
    // ...and the action runs afterwards, from an idle, so one that moves focus
    // itself (Settings dialog, Engine Log entry) is not overridden by that restore.
    Glib::signal_idle().connect_once([this, item]() {
        signal_item_run.emit(item.entry.id);
        if (item.run)
            item.run();
    });
}
