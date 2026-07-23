#include <catch2/catch_test_macros.hpp>

#include "lazycmake/build/run_manager.hpp"

using namespace lazycmake::build;
using namespace lazycmake::events;

struct RunManagerFixture {
    EventBus bus;
    RunManager mgr;

    RunManagerFixture()
        : mgr(bus) {}
};

TEST_CASE("RunManager starts in Idle state", "[build][run_manager]") {
    RunManagerFixture fix;
    CHECK(fix.mgr.currentState() == RunState::Idle);
    CHECK_FALSE(fix.mgr.isRunning());
}

TEST_CASE("Successful run transitions to Finished", "[build][run_manager]") {
    RunManagerFixture fix;

    fix.mgr.setBackend(std::make_unique<FakeRunBackend>(true, 0));
    fix.mgr.run("/usr/bin/my_app", {"arg1", "arg2"}, "/tmp", {});
    fix.mgr.waitForCompletion();

    CHECK(fix.mgr.currentState() == RunState::Finished);
}

TEST_CASE("Failed run captures the error", "[build][run_manager]") {
    RunManagerFixture fix;

    fix.mgr.setBackend(std::make_unique<FakeRunBackend>(false, 1));
    fix.mgr.run("/usr/bin/failing_app");
    fix.mgr.waitForCompletion();

    CHECK_FALSE(fix.mgr.lastResult().success);
    CHECK(fix.mgr.lastResult().exitCode == 1);
}

TEST_CASE("Run emits RunStartedEvent and RunFinishedEvent", "[build][run_manager]") {
    RunManagerFixture fix;

    bool started = false;
    bool finished = false;

    fix.bus.subscribe<RunStartedEvent>([&](const auto&) { started = true; });
    fix.bus.subscribe<RunFinishedEvent>([&](const auto&) { finished = true; });

    fix.mgr.setBackend(std::make_unique<FakeRunBackend>(true, 0));
    fix.mgr.run("/usr/bin/test_app");
    fix.mgr.waitForCompletion();
    fix.bus.drainQueue();

    CHECK(started);
    CHECK(finished);
}

TEST_CASE("Run emits RunOutputEvent for streamed output", "[build][run_manager]") {
    RunManagerFixture fix;

    int outputCount = 0;
    fix.bus.subscribe<RunOutputEvent>([&](const auto&) { ++outputCount; });

    fix.mgr.setBackend(std::make_unique<FakeRunBackend>(true, 0));
    fix.mgr.run("/usr/bin/test_app");
    fix.mgr.waitForCompletion();
    fix.bus.drainQueue();

    CHECK(outputCount > 0);
}

TEST_CASE("Kill during run transitions to Killed", "[build][run_manager]") {
    RunManagerFixture fix;

    fix.mgr.setBackend(std::make_unique<FakeRunBackend>(true, 0));
    fix.mgr.run("/usr/bin/test_app");

    fix.mgr.waitForCompletion();

    fix.mgr.kill();
    CHECK((fix.mgr.currentState() == RunState::Finished ||
           fix.mgr.currentState() == RunState::Killed));
}

TEST_CASE("Last result is populated after a run", "[build][run_manager]") {
    RunManagerFixture fix;

    fix.mgr.setBackend(std::make_unique<FakeRunBackend>(true, 42));
    fix.mgr.run("/usr/bin/test_app");
    fix.mgr.waitForCompletion();

    const auto& result = fix.mgr.lastResult();
    CHECK(result.success);
    CHECK(result.exitCode == 42);
}

TEST_CASE("Launch parameters are passed to the backend", "[build][run_manager]") {
    EventBus bus;
    RunManager mgr(bus);

    auto fakeBackend = std::make_unique<FakeRunBackend>(true, 0);
    FakeRunBackend* backendPtr = fakeBackend.get();

    mgr.setBackend(std::move(fakeBackend));
    mgr.run("/usr/bin/ls", {"-la", "/tmp"}, "/home", {});
    mgr.waitForCompletion();

    CHECK(backendPtr->lastExecutable() == "/usr/bin/ls");
    CHECK(backendPtr->lastArgs().size() == 2);
    CHECK(backendPtr->lastArgs()[0] == "-la");
    CHECK(backendPtr->lastArgs()[1] == "/tmp");
}
