#include "BWTest.h"
#include "DoNothingModule.h"

#include "BuildingPlacement.h"
#include "Map.h"

namespace
{
    void generateWalls()
    {
        std::vector<long> wallHeatmap(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);

        for (auto base : Map::allStartingLocations())
        {
            auto wall = BuildingPlacement::createForgeGatewayWall(true, base);
            if (wall.isValid())
            {
                std::cout << "Generated wall: " << wall << std::endl;

                wall.addToHeatmap(wallHeatmap);
            }
            else
            {
                std::cout << "ERROR: No wall available" << std::endl;
            }
        }

        CherryVis::addHeatmap("Wall", wallHeatmap, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
    }
}

TEST(ForgeGatewayWalls, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;

        test.onFrameMine = []() {
            if (currentFrame == 0)
            {
                generateWalls();
            }
        };
        test.run();
    });
}

TEST(ForgeGatewayWalls, Benzene8Oclock)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Benzene");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls();
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, NeoMoonGlaive)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Glaive");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls();
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Destination)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Destination");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls();
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Python)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Python");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls();
        }
    };
    test.run();
}
