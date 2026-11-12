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
        Gtk::ButtonsType::YES_NO,
        /*modal=*/true);
    dialog->set_secondary_text(i18n::tr("This cannot be undone."));
    return dialog;
}
