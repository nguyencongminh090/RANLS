// UI-05: the direction/category tag ([SEND]/[MESSAGE]/[OUTPUT]/…) is rendered
// in a separate, non-selectable gutter column, NOT in the Engine Log's text.
// The gutter drawing itself is a pure-GTK concern (a Gtk::DrawingArea draw
// callback) and cannot be exercised without a display server, so this test
// pins the model-level contract the view depends on: the string BottomPanel
// feeds into the TextView buffer for each line is the raw engine payload
// only, with the prefix kept out of it. If this holds, selecting log rows and
// copying yields payload with no "[SEND]"/"[MESSAGE]" prefixes.

#include "vendor/doctest.h"

#include "ui/engine_log_model.h"

TEST_CASE("UI-05: clipboard text for a log line is the raw payload, no gutter prefix") {
    EngineLogLine send{"[SEND]", "START 15", LogTagKind::Send};
    EngineLogLine msg{"[MESSAGE]", "depth 12 ev 30 pv h8 i9", LogTagKind::RecvMessage};
    EngineLogLine out{"[OUTPUT]", "8,8", LogTagKind::RecvOutput};

    CHECK(logLineClipboardText(send) == "START 15");
    CHECK(logLineClipboardText(msg) == "depth 12 ev 30 pv h8 i9");
    CHECK(logLineClipboardText(out) == "8,8");

    // Never carries the bracketed direction/category tag.
    for (const auto &l : {send, msg, out}) {
        CHECK(logLineClipboardText(l).find(l.prefix) == std::string::npos);
        CHECK(logLineClipboardText(l).find('[') == std::string::npos);
    }
}

TEST_CASE("UI-05: a copied multi-line selection is payloads joined by newlines only") {
    EngineLogModel model(5000);
    model.push({"[SEND]", "BEGIN", LogTagKind::Send});
    model.push({"[MESSAGE]", "thinking", LogTagKind::RecvMessage});
    model.push({"[OUTPUT]", "7,7", LogTagKind::RecvOutput});

    // Mirror how BottomPanel builds the TextView buffer contents.
    std::string buffer;
    bool first = true;
    for (const auto &line : model.lines()) {
        if (!first)
            buffer += '\n';
        buffer += logLineClipboardText(line);
        first = false;
    }

    CHECK(buffer == "BEGIN\nthinking\n7,7");
    CHECK(buffer.find("[SEND]") == std::string::npos);
    CHECK(buffer.find("[MESSAGE]") == std::string::npos);
}
