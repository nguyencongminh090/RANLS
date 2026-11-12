#include "ui/confirm_dialog.h"

#include "i18n/i18n.h"

Gtk::MessageDialog *makeDiscardConfirmDialog(Gtk::Window &parent, const Glib::ustring &action)
{
    auto *dialog = new Gtk::MessageDialog(
        parent,
        i18n::format(i18n::tr("%s will discard the current game (board, move history, and variation tree). Continue?"),
                     action.c_str()),
        /*use_markup=*/false,
        Gtk::MessageType::WARNING,
        Gtk::ButtonsType::NONE,
        /*modal=*/true);
    // No left of Yes, matching GTK's own YES_NO order. Deliberately no default response.
    dialog->add_button(i18n::tr("No"), Gtk::ResponseType::NO);
    dialog->add_button(i18n::tr("Yes"), Gtk::ResponseType::YES);
    dialog->set_secondary_text(i18n::tr("This cannot be undone."));
    return dialog;
}
