#include "about_dialog.h"

#include "build_info.h"
#include "version.h"

#include <cairo.h>

namespace {

// Small helper: a left-aligned "key" label for the build-info grid, dimmed
// via GTK's built-in `.dim-label` style class (no hard-coded colours — works
// in light and dark themes).
Gtk::Label *makeKeyLabel(const Glib::ustring &text)
{
    auto *lbl = Gtk::make_managed<Gtk::Label>(text);
    lbl->set_halign(Gtk::Align::START);
    lbl->set_xalign(0.0f);
    lbl->add_css_class("dim-label");
    return lbl;
}

// A left-aligned "value" label; `markup` linkifies embedded <a href> anchors.
Gtk::Label *makeValueLabel(const Glib::ustring &text, bool markup = false)
{
    auto *lbl = Gtk::make_managed<Gtk::Label>();
    lbl->set_halign(Gtk::Align::START);
    lbl->set_xalign(0.0f);
    lbl->set_selectable(!markup);
    if (markup)
        lbl->set_markup(text);
    else
        lbl->set_text(text);
    return lbl;
}

// A titled sub-section: a bold heading followed by its body widget.
Gtk::Box *makeSection(const Glib::ustring &title, Gtk::Widget &body)
{
    auto *box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 4);
    auto *heading = Gtk::make_managed<Gtk::Label>();
    heading->set_markup("<b>" + title + "</b>");
    heading->set_halign(Gtk::Align::START);
    heading->set_xalign(0.0f);
    box->append(*heading);
    box->append(body);
    return box;
}

}  // namespace

// ═════════════════════════════════════════════════════════════════════════════
AboutDialog::AboutDialog(Gtk::Window &parent)
{
    set_title("About RANLS");
    set_transient_for(parent);
    set_modal(true);
    set_resizable(true);
    set_default_size(560, 0);

    auto *outer = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 16);
    outer->set_margin(20);
    set_child(*outer);

    // ── Two-pane row: icon on the left, info column on the right ──────────
    auto *row = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 20);
    outer->append(*row);

    // Left: app icon. No bespoke logo asset ships yet (UI-11 boundary — do
    // not commit a new binary), so fall back to a themed stock icon.
    auto *logo = Gtk::make_managed<Gtk::Image>();
    logo->set_from_icon_name("applications-games-symbolic");
    logo->set_pixel_size(96);
    logo->set_valign(Gtk::Align::START);
    row->append(*logo);

    // Right: vertical info column.
    auto *info = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 6);
    info->set_hexpand(true);
    row->append(*info);

    auto *name = Gtk::make_managed<Gtk::Label>("RANLS");
    name->set_halign(Gtk::Align::START);
    name->set_xalign(0.0f);
    name->add_css_class("title-1");
    info->append(*name);

    auto *version = Gtk::make_managed<Gtk::Label>(Glib::ustring("Version ") + APP_VERSION);
    version->set_halign(Gtk::Align::START);
    version->set_xalign(0.0f);
    version->add_css_class("dim-label");
    version->set_selectable(true);
    info->append(*version);

    auto *tagline = Gtk::make_managed<Gtk::Label>("Professional Gomoku / Renju analysis tool");
    tagline->set_halign(Gtk::Align::START);
    tagline->set_xalign(0.0f);
    tagline->set_margin_bottom(4);
    info->append(*tagline);

    // ── Developer credit ────────────────────────────────────────────────
    {
        auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 2);
        body->append(*makeValueLabel("Developer: Nguyen Minh"));
        body->append(*makeValueLabel("Copyright © 2026 Nguyen Minh"));
        info->append(*makeSection("Developer", *body));
    }

    // ── Tech / build info ───────────────────────────────────────────────
    {
        auto *grid = Gtk::make_managed<Gtk::Grid>();
        grid->set_row_spacing(3);
        grid->set_column_spacing(12);
        int r = 0;
        auto addRow = [&](const Glib::ustring &k, const Glib::ustring &v) {
            grid->attach(*makeKeyLabel(k), 0, r);
            grid->attach(*makeValueLabel(v), 1, r);
            ++r;
        };
        addRow("GTK", Glib::ustring::format(GTK_MAJOR_VERSION, '.', GTK_MINOR_VERSION,
                                            '.', GTK_MICRO_VERSION));
        addRow("gtkmm", Glib::ustring::format(GTKMM_MAJOR_VERSION, '.', GTKMM_MINOR_VERSION,
                                              '.', GTKMM_MICRO_VERSION));
        addRow("Cairo", cairo_version_string());
        addRow("Build date", APP_BUILD_DATE);
        addRow("Commit", APP_GIT_COMMIT);
        addRow("License", "BSD-style — see LICENSE.md");
        info->append(*makeSection("Tech / build info", *grid));
    }

    // ── Links & protocol ───────────────────────────────────────────────
    {
        auto *body = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 3);
        body->append(*makeValueLabel(
            "Repository: <a href=\"https://github.com/nguyencongminh090/RANLS\">"
            "github.com/nguyencongminh090/RANLS</a>", true));
        body->append(*makeValueLabel(
            "Engine protocol: <a href=\"https://github.com/accreator/Yixin-protocol/blob/master/protocol.pdf\">"
            "Gomocup / Yixin protocol</a>", true));
        body->append(*makeValueLabel(
            "Supported engines: Rapfi, Yixin, and any Gomocup / "
            "Yixin-protocol-compatible engine"));
        info->append(*makeSection("Links & protocol", *body));
    }

    // ── Close button, bottom-right ─────────────────────────────────────
    auto *btnRow = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 0);
    btnRow->set_halign(Gtk::Align::END);
    auto *close = Gtk::make_managed<Gtk::Button>("Close");
    close->add_css_class("suggested-action");
    close->signal_clicked().connect([this]() { set_visible(false); });
    btnRow->append(*close);
    outer->append(*btnRow);

    // Esc closes the dialog too.
    auto key = Gtk::EventControllerKey::create();
    key->signal_key_pressed().connect(
        [this](guint keyval, guint, Gdk::ModifierType) {
            if (keyval == GDK_KEY_Escape) {
                set_visible(false);
                return true;
            }
            return false;
        },
        false);
    add_controller(key);
}
