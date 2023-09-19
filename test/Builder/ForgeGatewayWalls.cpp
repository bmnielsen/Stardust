#include "BWTest.h"
#include "DoNothingModule.h"

#include "BuildingPlacement.h"
#include "Map.h"
#include "Geo.h"
#include "Units.h"
#include "Strategist.h"
#include "DoNothingStrategyEngine.h"
#include "Workers.h"

namespace
{
    void generateWalls(int startLocationX = -1,
                       int startLocationY = -1,
                       std::vector<ForgeGatewayWall> *walls = nullptr)
    {
        std::vector<long> wallHeatmap(BWAPI::Broodwar->mapWidth() * BWAPI::Broodwar->mapHeight(), 0);

        for (auto base : Map::allStartingLocations())
        {
            if (startLocationX != -1 && startLocationX != base->getTilePosition().x) continue;
            if (startLocationY != -1 && startLocationY != base->getTilePosition().y) continue;

            auto wall = BuildingPlacement::createForgeGatewayWall(true, base);
            if (wall.isValid())
            {
                std::cout << base->getTilePosition() << ": " << wall << std::endl;

                wall.addToHeatmap(wallHeatmap);

                if (walls) walls->push_back(wall);
            }
            else
            {
                std::cout << base->getTilePosition() << ": ERROR: No wall available" << std::endl;
            }
        }

        CherryVis::addHeatmap("Wall", wallHeatmap, BWAPI::Broodwar->mapWidth(), BWAPI::Broodwar->mapHeight());
    }

    void createWallBuildings(const ForgeGatewayWall &wall)
    {
        auto getNearestBase = [&wall]()
        {
            Base *bestBase = nullptr;
            int bestDist = INT_MAX;
            for (auto base : Map::allBases())
            {
                int dist = wall.gapCenter.getApproxDistance(base->getPosition());
                if (dist < bestDist)
                {
                    bestBase = base;
                    bestDist = dist;
                }
            }
            return bestBase;
        };

        if (currentFrame == 10)
        {
            auto natural = getNearestBase();

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, natural->getPosition());
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, wall.gapCenter);
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(wall.pylon));

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->enemy(),
                                        BWAPI::UnitTypes::Zerg_Overlord,
                                        BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()) + BWAPI::Position(64, 48));
        }

        if (currentFrame == 20)
        {
            auto natural = getNearestBase();

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Nexus,
                                        Geo::CenterOfUnit(natural->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Pylon,
                                        Geo::CenterOfUnit(wall.pylon, BWAPI::UnitTypes::Protoss_Pylon));

            if (wall.naturalCannons.empty())
            {
                auto defensivePositions = BuildingPlacement::baseStaticDefenseLocations(natural);
                if (defensivePositions.isValid() && defensivePositions.powerPylon != wall.pylon)
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Pylon,
                                                Geo::CenterOfUnit(defensivePositions.powerPylon, BWAPI::UnitTypes::Protoss_Pylon));
                }
            }
        }

        if (currentFrame == 30)
        {
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Forge,
                                        Geo::CenterOfUnit(wall.forge, BWAPI::UnitTypes::Protoss_Forge));
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Gateway,
                                        Geo::CenterOfUnit(wall.gateway, BWAPI::UnitTypes::Protoss_Gateway));
        }

        if (currentFrame == 40)
        {
            for (auto pos : wall.probeBlockingPositions)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Probe, pos);
            }
        }

        if (currentFrame == 50)
        {
            auto natural = getNearestBase();

            for (auto cannon : wall.cannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                            Geo::CenterOfUnit(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon));
            }
            for (auto cannon : wall.naturalCannons)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                            Geo::CenterOfUnit(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon));
            }
            if (wall.naturalCannons.empty())
            {
                auto defensivePositions = BuildingPlacement::baseStaticDefenseLocations(natural);
                if (defensivePositions.isValid())
                {
                    for (auto cannon : defensivePositions.workerDefenseCannons)
                    {
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                    BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                    Geo::CenterOfUnit(cannon, BWAPI::UnitTypes::Protoss_Photon_Cannon));
                    }
                }
            }

        }
    }

    void configureBuildWallsTest(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 100;
        test.expectWin = false;
        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());
        };

        std::vector<ForgeGatewayWall> walls;

        test.onFrameMine = [&walls]() {
            if (currentFrame == 0)
            {
                generateWalls(-1, -1, &walls);
            }

            for (auto &wall : walls)
            {
                for (const auto &probe : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
                {
                    if (!wall.probeBlockingPositions.contains(probe->lastPosition)) continue;

                    Workers::reserveWorker(probe);
                    probe->stop();
                }

                createWallBuildings(wall);
            }
        };
    }

    void configureLingTightTest(BWTest &test)
    {
        test.opponentRace = BWAPI::Races::Zerg;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 1000;
        test.expectWin = false;
        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());
        };
        bool failed = false;
        test.onEndMine = [&test](bool)
        {
            test.addClockPositionToReplayName();
        };
        test.onFrameMine = [&failed, &test]()
        {
            auto &wall = BuildingPlacement::getForgeGatewayWall();
            if (!wall.isValid())
            {
                std::cout << "No wall available" << std::endl;
                test.writeReplay = false;
                BWAPI::Broodwar->leaveGame();
                return;
            }

            // Order cannons and blocking probes to stop every frame
            for (auto &cannon : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Photon_Cannon))
            {
                cannon->stop();
            }
            for (const auto &probe : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (wall.probeBlockingPositions.find(probe->lastPosition) == wall.probeBlockingPositions.end()) continue;

                Workers::reserveWorker(probe);
                probe->stop();
            }

            createWallBuildings(wall);

            if (currentFrame == 60)
            {
                auto createProbesInBase = [](Base *base)
                {
                    if (!base) return;
                    for (int x = -2; x < 6; x++)
                    {
                        for (int y = 4; y < 6; y++)
                        {
                            auto tile = base->getTilePosition() + BWAPI::TilePosition(x, y);
                            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                        BWAPI::UnitTypes::Protoss_Probe,
                                                        BWAPI::Position(tile) + BWAPI::Position(16, 16));
                        }
                    }
                };
                createProbesInBase(Map::getMyMain());
                createProbesInBase(Map::getMyNatural());
            }

            if (!failed)
            {
                for (auto &ling : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Zergling))
                {
                    CherryVis::log() << BWAPI::Position(wall.forge) << " - " << ling->lastPosition;
                    if (ling->getDistance(Map::getMyMain()->getPosition()) < 200)
                    {
                        failed = true;
                        EXPECT_TRUE(false) << "Zergling entered our base";
                    }
                }
            }
        };
        test.onFrameOpponent = []()
        {
            BWAPI::Position enemyBase = BWAPI::Positions::Invalid;
            for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (enemy->getType() != BWAPI::UnitTypes::Protoss_Nexus) continue;

                bool isStartPosition = false;
                for (auto startPosition : BWAPI::Broodwar->getStartLocations())
                {
                    if (enemy->getTilePosition() == startPosition)
                    {
                        isStartPosition = true;
                        break;
                    }
                }
                if (isStartPosition)
                {
                    enemyBase = enemy->getPosition();
                }
            }
            if (enemyBase == BWAPI::Positions::Invalid) return;

            if (BWAPI::Broodwar->getFrameCount() == 30)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Zerg_Zergling,
                                            BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation() + BWAPI::TilePosition(0, 5)));
                BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Metabolic_Boost, 1);
            }

            if (BWAPI::Broodwar->getFrameCount() >= 40)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() != BWAPI::UnitTypes::Zerg_Zergling) continue;

                    if (unit->getDistance(enemyBase) < 150)
                    {
                        unit->stop();
                    }

                    if (BWAPI::Broodwar->getFrameCount() % 24 == 0)
                    {
                        unit->move(enemyBase);
                    }
                }
            }
        };
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

TEST(ForgeGatewayWalls, AllCOG)
{
    Maps::RunOnEach(Maps::Get("cog2022"), [](BWTest test)
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

TEST(ForgeGatewayWalls, AllAIIDE)
{
    Maps::RunOnEach(Maps::Get("aiide2023"), [](BWTest test)
    {
        configureBuildWallsTest(test);
        test.run();
    });
}

TEST(ForgeGatewayWalls, LingTightAllSSCAIT)
{
    Maps::RunOnEachStartLocation(Maps::Get("sscai"), [](BWTest test)
    {
        configureLingTightTest(test);
        test.run();
    });
}

TEST(ForgeGatewayWalls, LingTightAllCOG)
{
    Maps::RunOnEachStartLocation(Maps::Get("cog2022"), [](BWTest test)
    {
        configureLingTightTest(test);
        test.run();
    });
}

TEST(ForgeGatewayWalls, PamirPlateau)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Pamir");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 6);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, NeoSylphid2)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("NeoSylphid2");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(7, 91);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Eclipse)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Eclipse");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(116, 7);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, EmpireOfTheSun)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Empire");
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

TEST(ForgeGatewayWalls, Outsider)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Outsider");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(96, 7);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, MatchPoint)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Match");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(100, 14);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Benzene)
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

TEST(ForgeGatewayWalls, TauCross)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Tau");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 9);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Andromeda5)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Andromeda");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 119);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Andromeda1)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Andromeda");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 7);
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

TEST(ForgeGatewayWalls, Heartbreak9)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Heartbreak");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(7, 37);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, GrandLine1)
{
    BWTest test;
    test.map = Maps::GetOne("GrandLine");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 6);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Allegro1)
{
    BWTest test;
    test.map = Maps::GetOne("Allegro");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 6);
        }
    };
    test.run();
}

TEST(ForgeGatewayWalls, Andromeda1b)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("Andromeda");
    configureLingTightTest(test);
    test.run();
}

TEST(ForgeGatewayWalls, LingTightHeartbreak)
{
    BWTest test;
    test.randomSeed = 30202;
    test.map = Maps::GetOne("Heartbreak");
    configureLingTightTest(test);
    test.run();
}

TEST(ForgeGatewayWalls, LingTightAndromeda)
{
    BWTest test;
    test.randomSeed = 32598;
    test.map = Maps::GetOne("Andromeda");
    configureLingTightTest(test);
    test.run();
}

TEST(ForgeGatewayWalls, LingTightIcarus)
{
    BWTest test;
    test.randomSeed = 23125;
    test.map = Maps::GetOne("Icarus");
    configureLingTightTest(test);
    test.run();
}

TEST(ForgeGatewayWalls, LingTightMoonGlaive12)
{
    BWTest test;
    test.randomSeed = 31337;
    test.map = Maps::GetOne("Glaive");
    configureLingTightTest(test);
    test.run();
}

TEST(ForgeGatewayWalls, LingTightTau6)
{
    BWTest test;
    test.randomSeed = 65763;
    test.map = Maps::GetOne("Tau");
    configureLingTightTest(test);
    test.run();
}