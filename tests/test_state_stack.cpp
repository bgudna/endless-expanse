#include <doctest/doctest.h>

#include "core/StateStack.h"

#include <string>
#include <vector>

using namespace ee;

namespace {
struct Recorder : GameState {
    Recorder(std::string n, std::vector<std::string>& l, bool overlay = false, bool below = false)
        : name(std::move(n)), log(l), overlay_(overlay), below_(below) {}
    void onEnter() override { log.push_back(name + ":enter"); }
    void onExit() override { log.push_back(name + ":exit"); }
    void handleInput(const InputState&, StateStack&) override { log.push_back(name + ":input"); }
    void update(double, StateStack&) override { log.push_back(name + ":update"); }
    void render(double) override { log.push_back(name + ":render"); }
    bool isOverlay() const override { return overlay_; }
    bool updatesBelow() const override { return below_; }

    std::string name;
    std::vector<std::string>& log;
    bool overlay_, below_;
};
} // namespace

TEST_CASE("push/pop are deferred until applyPending") {
    std::vector<std::string> log;
    StateStack s;
    s.push(std::make_unique<Recorder>("A", log));
    CHECK(s.empty());
    s.applyPending();
    CHECK(s.size() == 1);
    s.pop();
    CHECK(s.size() == 1);
    s.applyPending();
    CHECK(s.empty());
    CHECK(log == std::vector<std::string>{"A:enter", "A:exit"});
}

TEST_CASE("replace exits old then enters new") {
    std::vector<std::string> log;
    StateStack s;
    s.push(std::make_unique<Recorder>("A", log));
    s.applyPending();
    log.clear();
    s.replace(std::make_unique<Recorder>("B", log));
    s.applyPending();
    CHECK(log == std::vector<std::string>{"A:exit", "B:enter"});
    CHECK(s.size() == 1);
}

TEST_CASE("only the top state receives input and update by default") {
    std::vector<std::string> log;
    StateStack s;
    s.push(std::make_unique<Recorder>("A", log));
    s.push(std::make_unique<Recorder>("B", log));
    s.applyPending();
    log.clear();
    s.handleInput(InputState{});
    s.update(0.016);
    CHECK(log == std::vector<std::string>{"B:input", "B:update"});
}

TEST_CASE("overlay renders states beneath it; updatesBelow propagates updates") {
    std::vector<std::string> log;
    StateStack s;
    s.push(std::make_unique<Recorder>("A", log));
    s.push(std::make_unique<Recorder>("B", log, /*overlay*/ true, /*below*/ true));
    s.applyPending();
    log.clear();
    s.render(0.0);
    s.update(0.016);
    CHECK(log == std::vector<std::string>{"A:render", "B:render", "B:update", "A:update"});
}

TEST_CASE("clear exits all states top-down") {
    std::vector<std::string> log;
    StateStack s;
    s.push(std::make_unique<Recorder>("A", log));
    s.push(std::make_unique<Recorder>("B", log));
    s.applyPending();
    log.clear();
    s.clear();
    s.applyPending();
    CHECK(log == std::vector<std::string>{"B:exit", "A:exit"});
    CHECK(s.empty());
}
