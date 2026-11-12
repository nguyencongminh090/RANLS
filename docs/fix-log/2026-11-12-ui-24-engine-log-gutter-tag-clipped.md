# Engine Log gutter clipped the "MESSAGE" tag (2026-11-12)

**Status:** Active
**Related:** UI-24

## Prompt

User: "Write UI-24". Filed 2026-11-11 from the UI-23 verification pass: in the Engine Log the gutter
tag "MESSAGE" lost its last letter (baseline) or touched the log text (after UI-23's font change).

## Root cause

`BottomPanel::drawGutter` (`src/ui/bottom_panel.cpp`) sized the gutter with
`gutterArea_.set_size_request(width("MESSAGE") + 12, -1)` and drew each tag at x = 6. In GTK4 a size
request covers the widget's CSS padding and border. `.engine-gutter` has `padding: 0 4px;
border-right: 1px`, so the draw area was 9px narrower than requested. Instrumented values (live app,
dark theme): probe width 52 → request 64 → draw-callback/allocated content width **55**. The tag (52px
from x=6) ended at 58, so 3px were clipped.

## Fix

`gutterArea_.set_content_width(gutterWidth_)` instead of `set_size_request`. `Gtk::DrawingArea`'s
content width excludes CSS padding/border, which is what the x=6 + width + gap arithmetic assumes. One
line. The gutter-as-sibling design (UI-05), tag set, colours and flushing are unchanged. The
lazily-computed width is still measured once (not re-measured on a later font change). That was
outside this report and is not changed.

## Verification

- New `tests/test_ui24_gutter_tag_fits.cpp` loads the real `style.css` (new `STYLE_CSS_PATH` define;
  the test binary has no GResource) for the test's lifetime only, shows the Engine Log, appends
  "Message" lines, and checks gutter content width ≥ 6 + tag width + 4. Failed before (content 55,
  tag 52), passes after (content 64).
- `ctest` 5/5 under Xvfb. Live screenshot (dark, analysing): full "MESSAGE" with a gap and the
  separator before the log text, no GTK warnings.

## Knowledge

`systematic-debugging` skill; `.claude/rules/bugfix.md`; `gtk-ui-design` skill (DrawingArea).
