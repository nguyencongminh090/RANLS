#include "empty_state.h"

namespace {
// UX-01: opacity applied to the widget's themed foreground color for
// placeholder text/scaffold. Chosen so the blended color still clears the
// WCAG 4.5:1 contrast minimum against both Adwaita light (~#fafafa bg,
// ~#1e1e1e fg) and dark (~#242424 bg, ~#eeeeec fg) panel backgrounds used
// elsewhere in this app (see docs/fix-log/2026-08-21-ux-01-empty-states.md
// for the actual contrast-ratio numbers): at 0.65 alpha this comes out to
// ~5.1:1 (light) / ~6.5:1 (dark), both above the 4.5:1 floor with margin.
constexpr double kPlaceholderAlpha = 0.65;
}  // namespace

void EmptyState::drawPlaceholder(const Cairo::RefPtr<Cairo::Context> &cr,
                                  Gtk::Widget &widget,
                                  int width,
                                  int height,
                                  const std::string &text)
{
    if (width <= 0 || height <= 0) return;

    Gdk::RGBA fg = widget.get_color();

    auto layout = widget.create_pango_layout(text);
    layout->set_alignment(Pango::Alignment::CENTER);
    layout->set_width(static_cast<int>((width - 24) * Pango::SCALE));
    layout->set_wrap(Pango::WrapMode::WORD_CHAR);

    int textW = 0, textH = 0;
    layout->get_pixel_size(textW, textH);

    double x = (width - textW) / 2.0;
    double y = (height - textH) / 2.0;

    cr->save();
    cr->set_source_rgba(fg.get_red(), fg.get_green(), fg.get_blue(), kPlaceholderAlpha);
    cr->move_to(x, y);
    layout->show_in_cairo_context(cr);
    cr->restore();
}

EmptyStateOverlay::EmptyStateOverlay(const std::string &message)
{
    label_.set_text(message);
    label_.set_wrap(true);
    label_.set_wrap_mode(Pango::WrapMode::WORD_CHAR);
    label_.set_justify(Gtk::Justification::CENTER);
    label_.set_halign(Gtk::Align::CENTER);
    label_.set_valign(Gtk::Align::CENTER);
    label_.set_margin(16);
    label_.add_css_class("empty-state-message");
    label_.set_can_target(false);  // Never intercepts clicks meant for the content below.

    add_overlay(label_);
    set_measure_overlay(label_, true);
}

void EmptyStateOverlay::setContent(Gtk::Widget &content)
{
    set_child(content);
}

void EmptyStateOverlay::setEmpty(bool empty)
{
    label_.set_visible(empty);
}
