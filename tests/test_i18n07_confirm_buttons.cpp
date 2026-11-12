// I18N-07 — the "discard the current game?" confirmation must label its YES/NO
// response buttons through i18n::tr (UI language), not GTK's stock YES_NO labels.
// Links gtkmm; self-skips with no display server.

#include "vendor/doctest.h"

#include <gtkmm.h>

#include "i18n/i18n.h"
#include "ui/confirm_dialog.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>

namespace {

bool gtkReady() { return gtk_init_check(); }

std::string slurp(const std::string &path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string labelOf(Gtk::MessageDialog &d, Gtk::ResponseType r)
{
    auto *btn = dynamic_cast<Gtk::Button *>(d.get_widget_for_response(static_cast<int>(r)));
    REQUIRE(btn != nullptr);
    // GTK stock labels carry a mnemonic underscore ("_Yes"); ignore it.
    std::string t = btn->get_label();
    t.erase(std::remove(t.begin(), t.end(), '_'), t.end());
    return t;
}

struct Lang {
    Lang()
    {
        const std::string vi = slurp(I18N_VI_PATH);
        i18n::setCatalogLoader([vi](std::string_view code) { return code == "vi" ? vi : std::string(); });
    }
    ~Lang()
    {
        i18n::setCatalogLoader(nullptr);
        i18n::setLanguage("en");
    }
};

}  // namespace

TEST_CASE("I18N-07 confirm dialog buttons follow the UI language (vi translated, en unchanged)")
{
    if (!gtkReady()) { MESSAGE("no display: skipped"); return; }
    Lang lang;
    Gtk::Window parent;

    i18n::setLanguage("vi");
    REQUIRE(i18n::tr("Yes") == "Có");   // catalog entries exist, not the English fallthrough
    REQUIRE(i18n::tr("No") == "Không");
    {
        std::unique_ptr<Gtk::MessageDialog> d(makeDiscardConfirmDialog(parent, "x"));
        CHECK(labelOf(*d, Gtk::ResponseType::YES) == i18n::tr("Yes"));
        CHECK(labelOf(*d, Gtk::ResponseType::NO) == i18n::tr("No"));
    }

    i18n::setLanguage("en");
    {
        std::unique_ptr<Gtk::MessageDialog> d(makeDiscardConfirmDialog(parent, "x"));
        CHECK(labelOf(*d, Gtk::ResponseType::YES) == "Yes");
        CHECK(labelOf(*d, Gtk::ResponseType::NO) == "No");
    }
}
