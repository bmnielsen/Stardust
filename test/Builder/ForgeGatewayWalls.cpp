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
        auto getNatural = [&wall]()
        {
            Base *bestBase = nullptr;
            int bestDist = INT_MAX;
            for (auto base : Map::allStartingLocations())
            {
                int dist = wall.gapCenter.getApproxDistance(base->getPosition());
                if (dist < bestDist)
                {
                    bestBase = base;
                    bestDist = dist;
                }
            }

            auto natural = Map::mapSpecificOverride()->naturalForWallPlacement(bestBase);
            if (!natural) natural = Map::getStartingBaseNatural(bestBase);
            return natural;
        };

        if (currentFrame == 10)
        {
            auto natural = getNatural();

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, natural->getPosition());
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, wall.gapCenter);
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, BWAPI::Position(wall.pylon));

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->enemy(),
                                        BWAPI::UnitTypes::Zerg_Overlord,
                                        BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()) + BWAPI::Position(64, 48));
        }

        if (currentFrame == 20)
        {
            auto natural = getNatural();

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
            auto natural = getNatural();

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

    void configureBuildWallsTest(BWTest &test, std::vector<ForgeGatewayWall> &walls)
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
        std::vector<ForgeGatewayWall> walls;
        configureBuildWallsTest(test, walls);
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

TEST(ForgeGatewayWalls, Jade)
{
    BWTest test;
    test.randomSeed = 1617;
    test.map = Maps::GetOne("NeoJade");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.onFrameMine = []() {
        if (currentFrame == 0)
        {
            generateWalls(117, 117);
        }
    };
    test.run();
}
