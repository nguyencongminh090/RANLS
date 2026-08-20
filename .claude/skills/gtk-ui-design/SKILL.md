---
name: gtk-ui-design
description: GTK4/gtkmm4 UI implementation patterns for YixinBoard — custom Cairo-drawn widgets, Gio::ListStore + ColumnView data views, signal-based widget wiring. Use when adding or modifying a GTK widget in src/ui/, deciding how to implement a new UI element, or debugging GTK4-specific behavior (draw_func, ListStore, RefPtr). Not for whether the UI is usable (see ui-ux-review) or where UI code should live relative to model/engine (see software-architecture).
---

# GTK4/gtkmm4 UI implementation — YixinBoard

**This app is on GTK4** (`pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-4.0)` in
`CMakeLists.txt:15`, `#include <gtkmm.h>` throughout `src/ui/`), despite `README.md` still claiming
"migrated to GTK3" — that's a stale doc, not the current target. Write GTK4/gtkmm4 idiom, not GTK3.
(Worth filing a `docs/fix-log` or `TODO.md` entry to correct the README — see `/CLAUDE.md`.)

## Two established custom-widget patterns in this codebase — follow them, don't invent a third

### Pattern A: Cairo-drawn widget (`Gtk::DrawingArea`)

Used by `BoardRenderer`/`BoardView` (`src/ui/board_renderer.cpp`, `board_view.cpp`) and
`TreeNodeView` (`src/ui/tree_node_view.cpp`). GTK4 shape, not GTK3's `on_draw` override:

```cpp
class TreeNodeView : public Gtk::ScrolledWindow {
    void onDraw(const Cairo::RefPtr<Cairo::Context> &cr, int width, int height);
    Gtk::DrawingArea drawArea_;
    // constructor wires: drawArea_.set_draw_func(sigc::mem_fun(*this, &TreeNodeView::onDraw));
};
```

Rules for this pattern:
- The widget owning the drawing state is a `Gtk::ScrolledWindow`/`Gtk::Box` etc. wrapping an inner
  `Gtk::DrawingArea` — don't subclass `DrawingArea` directly if the widget needs scroll or layout
  children around it (see `TreeNodeView`, `BoardView`).
- Layout computation (`layoutTree` in `TreeNodeView`) is separated from drawing (`onDraw`) — compute
  positions into a member vector (`nodes_`) in `update()`, then have `onDraw` only paint from that
  cache. Don't recompute layout inside the draw callback; it runs on every repaint.
- Hit-testing (hover/click) reads back from the same cached layout (`hoverIndex_`, `NodeLayout`) —
  don't re-derive geometry from raw model data in the event handler.

### Pattern B: Data-bound list (`Gtk::ColumnView` + `Gio::ListStore`)

Used by `TreeExplorer` (`src/ui/tree_explorer.h`). GTK4's list-view model, not GTK3's `Gtk::TreeView`
+ `Gtk::TreeStore`:

```cpp
struct RowData : public Glib::Object {
    std::string moveStr, evalStr /* ... */;
    static Glib::RefPtr<RowData> create(/* fields */) {
        auto obj = Glib::make_refptr_for_instance<RowData>(new RowData());
        /* fill fields */
        return obj;
    }
};
Gtk::ColumnView                       columnView_;
Glib::RefPtr<Gio::ListStore<RowData>> store_;
Glib::RefPtr<Gtk::NoSelection>        selection_;
```

Rules:
- Row data is a `Glib::Object` subclass with a private constructor + a `static create()` factory
  returning `Glib::RefPtr<T>` — GTK4's ref-counted ownership, never construct one with `new` directly
  outside `create()`.
- `update()` rebuilds the whole store from the model on every call (see `TreeExplorer::update`,
  `TreeNodeView::update`) — this is the established pattern here, not a shortcut to fix. If a future
  screen needs incremental updates for a large data set, that's a deliberate deviation worth calling
  out, not silently different behavior (see `perf-optimization` skill on when it actually matters).
- Selection model (`Gtk::NoSelection` here, since click handling goes through a gesture/signal
  instead of ColumnView's own selection) — match what the existing view does unless the new widget
  genuinely needs GTK-managed selection.

## Signal wiring convention

All custom widgets expose `sigc::signal<...>` members (`signal_node_clicked`, `signal_node_selected`,
`signal_analyzing_state`, etc.) rather than taking callbacks in the constructor or exposing internal
GTK signals directly. New widgets should do the same — the composition root (`main_window.cpp`)
connects `sigc::mem_fun`/lambdas to these, never the reverse (a widget reaching into `MainWindow`).

## GTK4-specific gotchas to check when porting or extending

- `set_draw_func` replaces GTK3's `on_draw` virtual override — the callback signature is
  `(cr, width, height)`, not `(cr)` with a separate `get_allocated_width/height` call.
- `Gtk::Application`/`Gtk::ApplicationWindow` replace GTK3's `Gtk::Main` event loop — check
  `application.cpp`/`main.cpp` for the actual startup shape before assuming GTK3 idioms apply.
- CSS: GTK4's `Gtk::CssProvider` loading API differs from GTK3's; check `src/resources/style.css`
  loading code (likely in `application.cpp`) rather than assuming GTK3 syntax.
- `Glib::RefPtr` and `Gio::ListStore<T>::append/remove` replace manual `GtkTreeIter` manipulation —
  don't reach for GTK3-era `TreeIter` patterns anywhere in this codebase.
- Custom `DrawingArea`-based widgets (`BoardRenderer`, `TreeNodeView`) get no automatic focus ring or
  screen-reader semantics — GTK4's `Gtk::Accessible` interface (role + property updates via
  `update_property`) and a manually-drawn focus indicator in `onDraw` are how to close that gap when
  asked to implement it. See `ui-ux-review`'s "Accessibility" section for which widgets currently lack this.

## Scope boundary

This skill is implementation mechanics only. Whether a widget's behavior is actually usable is
`ui-ux-review`. Whether a new widget's logic belongs in `ui/` vs leaking into `model/` is
`software-architecture`.
