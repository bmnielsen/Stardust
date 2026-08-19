#include "BWTest.h"

#include "Trainers.h"

using namespace MiningOptimizationTraining;

TEST(InitializeMapData, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runInitializeMapData(test, {});
}

TEST(InitializeMapData, VermeerOnePatch)
{
    PathExplorationModuleOptions options;
    options.onePatch = BWAPI::TilePosition(2, 11);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runInitializeMapData(test, options);
}

TEST(InitializeMapData, VermeerOneBase)
{
    PathExplorationModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runInitializeMapData(test, options);
}

TEST(InitializeMapData, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runInitializeMapData(test, {});
    });
}

TEST(InitializeMapData, AllCOG2026)
{
    Maps::RunOnEach(Maps::Get("cog2026"), [&](BWTest test)
    {
        runInitializeMapData(test, {});
    });
}

TEST(InitializeMapData, Benzene)
{
    BWTest test;
    test.map = Maps::GetOne("Benzene");
    runInitializeMapData(test, {});
}

TEST(InitializeMapData, BenzeneOneBase)
{
    PathExplorationModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 96);

    BWTest test;
    test.map = Maps::GetOne("Benzene");
    runInitializeMapData(test, options);
}

TEST(InitializeMapData, Destination)
{
    BWTest test;
    test.map = Maps::GetOne("Destination");
    runInitializeMapData(test, {});
}

TEST(InitializeMapData, All)
{
    Maps::RunOnEach(Maps::Get(), [&](BWTest test)
    {
        PathExplorationModuleOptions options;
        options.skipIfMapDataExists = true;
        runInitializeMapData(test, options);
    });
}
