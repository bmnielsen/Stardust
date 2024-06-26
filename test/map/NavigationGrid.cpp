#include "BWTest.h"
#include "DoNothingModule.h"

#include "Units.h"
#include "Map.h"
#include "PathFinding.h"

namespace
{
    bool validateGrid(NavigationGrid &grid)
    {
        int minCostDiff = 32;
        int maxCostDiff = 45;

        for (int y = 0; y < BWAPI::Broodwar->mapHeight(); y++)
        {
            for (int x = 0; x < BWAPI::Broodwar->mapWidth(); x++)
            {
                auto &node = grid[BWAPI::TilePosition(x, y)];

                if (node.cost > 0 && node.cost < USHRT_MAX && !node.nextNode)
                {
                    std::cout << BWAPI::Broodwar->getFrameCount() << ": Node with no next node " << node << std::endl;
                    return false;
                }

                if (node.cost == 0 && node.nextNode)
                {
                    std::cout << BWAPI::Broodwar->getFrameCount() << ": Goal node with next node " << node << std::endl;
                    return false;
                }

                // Doesn't work as there are walkable areas behind doodads that can't be reached
                /*
                if (Map::unwalkableProximity(node.x, node.y) >= 2 && node.cost == USHRT_MAX &&
                    PathFinding::GetGroundDistance(BWAPI::Position(BWAPI::TilePosition(x, y)), BWAPI::Position(grid.goal)) != -1)
                {
                    std::cout << "Should be path from " << node << std::endl;
                    return false;
                }
                */

                if (node.nextNode)
                {
                    int costDiff = node.cost - node.nextNode->cost;
                    if (costDiff <= 0)
                    {
                        std::cout << BWAPI::Broodwar->getFrameCount() << ": Non-decreasing cost " << node << std::endl;
                        return false;
                    }

                    if (costDiff < minCostDiff) minCostDiff = costDiff;
                    if (costDiff > maxCostDiff) maxCostDiff = costDiff;
                }

                auto current = node.nextNode;
                int i = 0;
                while (current != nullptr && i < 5)
                {
                    if (current == &node)
                    {
                        std::cout << BWAPI::Broodwar->getFrameCount() << ": Loop between " << node << " and " << *current << std::endl;
                        return false;
                    }

                    if (!Map::isWalkable(current->x, current->y))
                    {
                        std::cout << BWAPI::Broodwar->getFrameCount() << ": Path from " << node << " goes through unwalkable tile " << *current
                                  << std::endl;
                        return false;
                    }

                    current = current->nextNode;
                    i++;
                }
            }
        }

        std::cout << BWAPI::Broodwar->getFrameCount() << ": minCostDelta=" << minCostDiff << "; maxCostDelta=" << maxCostDiff << std::endl;

        return true;
    }

    void setupGridTest(BWTest &test,
                       BWAPI::TilePosition goal,
                       NavigationGrid *&grid,
                       const std::string &map = "Fighting Spirit",
                       int randomSeed = 42)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.map = Maps::GetOne(map);
        test.randomSeed = randomSeed;
        test.frameLimit = 40;
        test.expectWin = false;
        test.writeReplay = true;

        test.onStartMine = [&grid, goal]()
        {
            CherryVis::initialize();
            Units::initialize();
            Map::initialize();
            PathFinding::initializeGrids();

            grid = new NavigationGrid(goal, BWAPI::UnitTypes::Protoss_Nexus.tileSize());
            EXPECT_TRUE(validateGrid(*grid));
        };

        test.onEndMine = [&](bool won)
        {
            CherryVis::gameEnd();

            EXPECT_TRUE(validateGrid(*grid));
        };
    }

    void addBuilding(NavigationGrid *grid, BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        std::cout << BWAPI::Broodwar->getFrameCount() << ": addBuilding: " << type << " @ " << tile << std::endl;

        Map::onBuildingLanded(type, tile);
        grid->addBlockingObject(tile, type.tileSize());
    }

    void removeBuilding(NavigationGrid *grid, BWAPI::UnitType type, BWAPI::TilePosition tile)
    {
        std::cout << BWAPI::Broodwar->getFrameCount() << ": removeBuilding: " << type << " @ " << tile << std::endl;

        Map::onBuildingLifted(type, tile);
        grid->removeBlockingObject(tile, type.tileSize());
    }

}

// Tests that a grid can be initialized to the enemy position
TEST(InitializeNavigationGrid, EnemyPosition)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 117), grid);

    test.run();
}

// Tests that a grid can be initialized to our main base
TEST(InitializeNavigationGrid, OurPosition)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 7), grid);

    test.run();
}

// Tests that adding a pylon to the grid works
TEST(UpdateNavigationGrid, Pylon)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 7), grid);

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 5) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(118, 11));
        grid->update();

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}

// Tests that adding a gateway to the grid works
TEST(UpdateNavigationGrid, Gateway)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 7), grid);

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 5) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(112, 13));
        grid->update();

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}

TEST(UpdateNavigationGrid, ManyBuildings)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 7), grid);

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 1) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(118, 11));
        if (BWAPI::Broodwar->getFrameCount() == 2) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(116, 13));
        if (BWAPI::Broodwar->getFrameCount() == 3) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(109, 6));
        if (BWAPI::Broodwar->getFrameCount() == 4) addBuilding(grid, BWAPI::UnitTypes::Protoss_Cybernetics_Core, BWAPI::TilePosition(115, 11));
        if (BWAPI::Broodwar->getFrameCount() == 5) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 7));
        if (BWAPI::Broodwar->getFrameCount() == 6) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(112, 13));
        if (BWAPI::Broodwar->getFrameCount() == 7) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(122, 15));
        if (BWAPI::Broodwar->getFrameCount() == 8) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(117, 18));
        if (BWAPI::Broodwar->getFrameCount() == 9) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(122, 1));
        if (BWAPI::Broodwar->getFrameCount() == 10) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 4));
        if (BWAPI::Broodwar->getFrameCount() == 11) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 20));
        if (BWAPI::Broodwar->getFrameCount() == 12) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(122, 1));
        if (BWAPI::Broodwar->getFrameCount() == 13) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 4));
        if (BWAPI::Broodwar->getFrameCount() == 14) addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(115, 113));
        if (BWAPI::Broodwar->getFrameCount() == 15) addBuilding(grid, BWAPI::UnitTypes::Zerg_Hydralisk_Den, BWAPI::TilePosition(113, 115));
        if (BWAPI::Broodwar->getFrameCount() == 16) addBuilding(grid, BWAPI::UnitTypes::Protoss_Nexus, BWAPI::TilePosition(88, 13));
        if (BWAPI::Broodwar->getFrameCount() == 17) addBuilding(grid, BWAPI::UnitTypes::Zerg_Creep_Colony, BWAPI::TilePosition(113, 112));
        if (BWAPI::Broodwar->getFrameCount() == 18) addBuilding(grid, BWAPI::UnitTypes::Zerg_Creep_Colony, BWAPI::TilePosition(115, 111));
        if (BWAPI::Broodwar->getFrameCount() == 19) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(111, 18));
        if (BWAPI::Broodwar->getFrameCount() == 20) addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(117, 109));
        if (BWAPI::Broodwar->getFrameCount() == 21) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(122, 17));
        if (BWAPI::Broodwar->getFrameCount() == 22) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(109, 8));
        if (BWAPI::Broodwar->getFrameCount() == 23) addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(117, 114));
        if (BWAPI::Broodwar->getFrameCount() == 24) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(122, 20));
        if (BWAPI::Broodwar->getFrameCount() == 25) addBuilding(grid, BWAPI::UnitTypes::Zerg_Creep_Colony, BWAPI::TilePosition(117, 110));
        if (BWAPI::Broodwar->getFrameCount() == 26) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(122, 22));
        if (BWAPI::Broodwar->getFrameCount() == 27) addBuilding(grid, BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(117, 21));
        if (BWAPI::Broodwar->getFrameCount() == 28) addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(109, 1));

        if (BWAPI::Broodwar->getFrameCount() == 30)
        {
            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 31) removeBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(117, 109));
        if (BWAPI::Broodwar->getFrameCount() == 32) removeBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(117, 114));
        if (BWAPI::Broodwar->getFrameCount() == 33) removeBuilding(grid, BWAPI::UnitTypes::Zerg_Creep_Colony, BWAPI::TilePosition(117, 110));
        if (BWAPI::Broodwar->getFrameCount() == 34) removeBuilding(grid, BWAPI::UnitTypes::Zerg_Hydralisk_Den, BWAPI::TilePosition(113, 115));

        if (BWAPI::Broodwar->getFrameCount() == 40)
        {
            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}

TEST(UpdateNavigationGrid, EnemyStaticDefense)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 117), grid);

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 1)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(116, 114));
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(118, 114));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(120, 114));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}

TEST(NavigationGrid, DestinationBlockedChoke)
{
    BWTest test;
    test.map = Maps::GetOne("Destination");
    test.frameLimit = 500;
    test.randomSeed = 42;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(34, 122))),
    };

    test.run();
}

TEST(UpdateNavigationGrid, BlockedMainChokeBug)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(117, 7), grid, "Andromeda");

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 1)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(412, 104)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 2)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(412, 96)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 3)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Pylon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(420, 96)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 4)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Nexus,
                        BWAPI::TilePosition(BWAPI::WalkPosition(412, 84)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Forge,
                        BWAPI::TilePosition(BWAPI::WalkPosition(428, 96)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 6)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(404, 104)));
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(396, 100)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 7)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(420, 104)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 8)
        {
            removeBuilding(grid,
                           BWAPI::UnitTypes::Protoss_Photon_Cannon,
                           BWAPI::TilePosition(BWAPI::WalkPosition(404, 104)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 9)
        {
            removeBuilding(grid,
                           BWAPI::UnitTypes::Protoss_Photon_Cannon,
                           BWAPI::TilePosition(BWAPI::WalkPosition(396, 100)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }
        if (BWAPI::Broodwar->getFrameCount() == 10)
        {
            addBuilding(grid,
                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                        BWAPI::TilePosition(BWAPI::WalkPosition(440, 100)));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}

TEST(UpdateNavigationGrid, PylonEmpireOfTheSun)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(7, 119), grid, "Empire", 79141);

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 2)
        {
            addBuilding(grid, BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 120));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
            EXPECT_TRUE((*grid)[BWAPI::TilePosition(118, 122)].cost < USHRT_MAX);
        }

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}

TEST(UpdateNavigationGrid, HeartbreakRidgeBug)
{
    BWTest test;
    NavigationGrid *grid;

    setupGridTest(test, BWAPI::TilePosition(7, 37), grid, "Heartbreak", 79141);

    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 2)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Hatchery, BWAPI::TilePosition(13, 66));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 3)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(17, 67));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 4)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(19, 67));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(21, 66));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 6)
        {
            removeBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(21, 66));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 7)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(21, 67));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
        }

        if (BWAPI::Broodwar->getFrameCount() == 8)
        {
            addBuilding(grid, BWAPI::UnitTypes::Zerg_Sunken_Colony, BWAPI::TilePosition(21, 65));

            grid->update();
            EXPECT_TRUE(validateGrid(*grid));
            EXPECT_TRUE((*grid)[BWAPI::TilePosition(32, 70)].cost < USHRT_MAX);
        }

        CherryVis::frameEnd(BWAPI::Broodwar->getFrameCount());
    };

    test.run();
}
