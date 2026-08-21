// Regression tests for RT-02: the engine log must be bounded and must drop
// only the oldest lines once over capacity, since BottomPanel trims its GTK
// buffer by exactly EngineLogModel::push()'s reported drop count to keep the
// on-screen buffer and this model in lockstep.

#include "vendor/doctest.h"

#include "ui/engine_log_model.h"

TEST_CASE("EngineLogModel: empty model") {
    EngineLogModel model(5000);
    CHECK(model.empty());
    CHECK(model.size() == 0);
    CHECK(model.maxLines() == 5000);
}

TEST_CASE("EngineLogModel: push under cap keeps everything, drops nothing") {
    EngineLogModel model(10);
    for (int i = 0; i < 5; ++i) {
        std::size_t dropped = model.push({"[SEND]", std::to_string(i), LogTagKind::Send});
        CHECK(dropped == 0);
    }
    CHECK(model.size() == 5);
}

TEST_CASE("EngineLogModel: pushing past the cap drops exactly the oldest line each time") {
    EngineLogModel model(3);
    for (int i = 0; i < 3; ++i)
        CHECK(model.push({"[RECV]", std::to_string(i), LogTagKind::RecvOutput}) == 0);

    // 4th push exceeds the cap by one -> exactly one line dropped.
    CHECK(model.push({"[RECV]", "3", LogTagKind::RecvOutput}) == 1);
    REQUIRE(model.size() == 3);
    CHECK(model.lines().front().text == "1");
    CHECK(model.lines().back().text == "3");
}

TEST_CASE("EngineLogModel: retains exactly the most recent window after many pushes") {
    EngineLogModel model(5000);
    for (int i = 0; i < 12000; ++i)
        model.push({"[RECV]", std::to_string(i), LogTagKind::RecvOutput});

    REQUIRE(model.size() == 5000);
    CHECK(model.lines().front().text == "7000");
    CHECK(model.lines().back().text == "11999");
}

TEST_CASE("EngineLogModel: setMaxLines shrinks immediately, dropping from the front") {
    EngineLogModel model(10);
    for (int i = 0; i < 10; ++i)
        model.push({"[SEND]", std::to_string(i), LogTagKind::Send});

    model.setMaxLines(3);
    REQUIRE(model.size() == 3);
    CHECK(model.lines().front().text == "7");
    CHECK(model.lines().back().text == "9");
}

TEST_CASE("EngineLogModel: clear empties the model but preserves the cap") {
    EngineLogModel model(10);
    model.push({"[SEND]", "x", LogTagKind::Send});
    model.clear();
    CHECK(model.empty());
    CHECK(model.maxLines() == 10);
}
