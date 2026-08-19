#include "BWTest.h"

#include "Trainers.h"

using namespace MiningOptimizationTraining;

TEST(PathExploration, VermeerSingleWorker)
{
    PathExplorationModuleOptions options;
    options.onePatch = BWAPI::TilePosition(2, 11);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runExplorePaths(test, options);
}

TEST(PathExploration, VermeerOneBase)
{
    PathExplorationModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runExplorePaths(test, options);
}

TEST(PathExploration, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runExplorePaths(test, {});
}

TEST(PathExploration, BenzeneOneBase)
{
    PathExplorationModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 96);

    BWTest test;
    test.map = Maps::GetOne("Benzene");
    runExplorePaths(test, options);
}

TEST(PathExploration, Benzene)
{
    BWTest test;
    test.map = Maps::GetOne("Benzene");
    runExplorePaths(test, {});
}

TEST(PathExploration, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runExplorePaths(test, {});
    });
}

TEST(PathExploration, AllCOG2026)
{
    Maps::RunOnEach(Maps::Get("cog2026"), [&](BWTest test)
    {
        runExplorePaths(test, {});
    });
}
