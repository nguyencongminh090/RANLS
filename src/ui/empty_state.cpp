#include "empty_state.h"

// UI-08: the placeholder message that UX-01 painted here has been removed —
// empty panels render clean with no instructional text. This wrapper now only
// forwards its content child.

EmptyStateOverlay::EmptyStateOverlay(const std::string &message)
{
    (void)message;
}

void EmptyStateOverlay::setContent(Gtk::Widget &content)
{
    set_child(content);
}

void EmptyStateOverlay::setEmpty(bool empty)
{
    (void)empty;
}
