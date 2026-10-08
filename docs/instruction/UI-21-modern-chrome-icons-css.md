# UI-21 — how
See `features/modern-ui/planning.md` sequencing. Touch points: `src/resources/{style.css,ranls.gresource.xml,icons/}`,
`src/application.cpp` (icon path), `src/main_window.{h,cpp}` (`buildMenuBar`, `buildToolbar`, `buildLayout`),
`src/ui/engine_status.cpp`, `CMakeLists.txt` (svg DEPENDS). New icons: filled paths only, 16x16, `fill-rule=evenodd`.
