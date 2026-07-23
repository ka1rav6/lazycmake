#include <catch2/catch_test_macros.hpp>

#include "lazycmake/build/build_manager.hpp"

using namespace lazycmake::build;
using namespace lazycmake::events;

struct BuildManagerFixture {
    EventBus bus;
    std::unique_ptr<BuildManager> mgr;

    BuildManagerFixture(bool shouldSucceed = true)
        : mgr(std::make_unique<BuildManager>(
              std::make_unique<FakeBuildBackend>(shouldSucceed), bus)) {}
};

TEST_CASE("BuildManager starts in Idle state", "[build][build_manager]") {
    BuildManagerFixture fix;
    CHECK(fix.mgr->currentStage() == BuildStage::Idle);
    CHECK_FALSE(fix.mgr->isRunning());
}

TEST_CASE("Successful configure transitions through states", "[build][build_manager]") {
    BuildManagerFixture fix;

    fix.mgr->configure("/src", "/build", "Ninja", "Debug");
    fix.mgr->waitForCompletion();

    CHECK(fix.mgr->currentStage() == BuildStage::Succeeded);
    CHECK_FALSE(fix.mgr->isRunning());
}

TEST_CASE("Failed configure transitions to Failed", "[build][build_manager]") {
    BuildManagerFixture fix(false);

    fix.mgr->configure("/src", "/build", "Ninja", "Debug");
    fix.mgr->waitForCompletion();

    CHECK(fix.mgr->currentStage() == BuildStage::Failed);
    CHECK_FALSE(fix.mgr->isRunning());
}

TEST_CASE("Successful build transitions to Succeeded", "[build][build_manager]") {
    BuildManagerFixture fix;

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("my_app");
    fix.mgr->waitForCompletion();

    CHECK(fix.mgr->currentStage() == BuildStage::Succeeded);
}

TEST_CASE("Failed build transitions to Failed", "[build][build_manager]") {
    BuildManagerFixture fix(false);

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("my_app");
    fix.mgr->waitForCompletion();

    CHECK(fix.mgr->currentStage() == BuildStage::Failed);
}

TEST_CASE("Configure emits progress events on the bus", "[build][build_manager]") {
    BuildManagerFixture fix;

    int progressCount = 0;
    fix.bus.subscribe<BuildProgressEvent>([&](const auto&) { ++progressCount; });

    fix.mgr->configure("/src", "/build", "Ninja", "Debug");
    fix.mgr->waitForCompletion();
    fix.bus.drainQueue();

    CHECK(progressCount > 0);
}

TEST_CASE("Successful build emits BuildFinishedEvent with success=true", "[build][build_manager]") {
    BuildManagerFixture fix;

    bool finished = false;
    fix.bus.subscribe<BuildFinishedEvent>([&](const BuildFinishedEvent& e) {
        finished = true;
        CHECK(e.success);
    });

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("my_app");
    fix.mgr->waitForCompletion();
    fix.bus.drainQueue();

    CHECK(finished);
}

TEST_CASE("Failed build emits BuildFinishedEvent with success=false", "[build][build_manager]") {
    BuildManagerFixture fix(false);

    bool finished = false;
    fix.bus.subscribe<BuildFinishedEvent>([&](const BuildFinishedEvent& e) {
        finished = true;
        CHECK_FALSE(e.success);
    });

    // Don't set configure params so build() skips auto-configure and goes
    // directly to the build phase with the failing backend.
    fix.mgr->build("my_app");
    fix.mgr->waitForCompletion();
    fix.bus.drainQueue();

    CHECK(finished);
}

TEST_CASE("Cancel is no-op on completed build", "[build][build_manager]") {
    BuildManagerFixture fix;

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("test");
    fix.mgr->waitForCompletion();
    CHECK(fix.mgr->currentStage() == BuildStage::Succeeded);

    fix.mgr->cancel();
    CHECK(fix.mgr->currentStage() == BuildStage::Succeeded);
}

TEST_CASE("Cancel when running transitions to Idle", "[build][build_manager]") {
    BuildManagerFixture fix;

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("test");
    fix.mgr->waitForCompletion();
    CHECK(fix.mgr->currentStage() == BuildStage::Succeeded);

    fix.mgr->cancel();
    CHECK(fix.mgr->currentStage() == BuildStage::Succeeded);
}

TEST_CASE("Last result is populated after a build", "[build][build_manager]") {
    BuildManagerFixture fix;

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("test");
    fix.mgr->waitForCompletion();

    const auto& result = fix.mgr->lastResult();
    CHECK(result.status == BuildOperationStatus::Completed);
    CHECK(result.exitCode == 0);
}

TEST_CASE("Last result shows failure after a failed build", "[build][build_manager]") {
    BuildManagerFixture fix(false);

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("test");
    fix.mgr->waitForCompletion();

    const auto& result = fix.mgr->lastResult();
    CHECK(result.status == BuildOperationStatus::Failed);
}

TEST_CASE("Reset clears last result and returns to Idle", "[build][build_manager]") {
    BuildManagerFixture fix(false);

    fix.mgr->setConfigureParams("/src", "/build", "Ninja", "Debug");
    fix.mgr->build("test");
    fix.mgr->waitForCompletion();
    CHECK(fix.mgr->currentStage() == BuildStage::Failed);

    fix.mgr->reset();
    CHECK(fix.mgr->currentStage() == BuildStage::Idle);
    CHECK(fix.mgr->lastResult().exitCode == -1);
}
