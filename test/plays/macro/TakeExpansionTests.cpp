#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeExpansion.h"
#include "Map.h"

namespace
{
    class TakeExpansionsStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            auto natural = Map::getMyNatural();
            if (natural->owner != BWAPI::Broodwar->self())
            {
                if (BWAPI::Broodwar->getFrameCount() > 3000) takeNaturalExpansion(plays, prioritizedProductionGoals);
                return;
            }

            auto base = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 103)));

            for (auto &play : plays)
            {
                if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(play))
                {
                    return;
                }
            }

            auto play = std::make_shared<TakeExpansion>(base, 0);
            plays.emplace(plays.begin(), play);
        }
    };

    class ZerglingHarassModule : public DoNothingModule
    {
        BWAPI::Unit builder = nullptr;

    public:

        void onFrame() override
        {
            if (BWAPI::Broodwar->getFrameCount() < 10) return;

            if (BWAPI::Broodwar->getFrameCount() == 10)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker())
                    {
                        if (!builder)
                        {
                            builder = unit;
                            continue;
                        }

                        auto mineral = BWAPI::Broodwar->getClosestUnit(unit->getPosition(), BWAPI::Filter::IsMineralField);
                        if (mineral)
                        {
                            unit->gather(mineral);
                        }
                    }
                }

                return;
            }

            if (builder && BWAPI::Broodwar->getFrameCount() % 24 == 0)
            {
                for (auto geyser : BWAPI::Broodwar->getGeysers())
                {
                    int dist = geyser->getDistance(builder);
                    if (dist < 32)
                    {
                        builder->build(BWAPI::UnitTypes::Zerg_Extractor, geyser->getTilePosition());
                    }
                    else if (dist < 500)
                    {
                        builder->move(geyser->getPosition());
                    }
                }
            }

            // Gather from the extractor when it finishes
            for (auto extractor : BWAPI::Broodwar->self()->getUnits())
            {
                if (!extractor->getType().isRefinery()) continue;
                if (!extractor->isCompleted()) continue;

                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker())
                    {
                        if (unit->isGatheringMinerals())
                        {
                            unit->gather(extractor);
                        }
                    }

                    if (unit->getType().isResourceDepot() && !unit->isResearching())
                    {
                        unit->research(BWAPI::TechTypes::Burrowing);
                    }
                }
            }

            for (auto zergling : BWAPI::Broodwar->self()->getUnits())
            {
                if (zergling->getType() != BWAPI::UnitTypes::Zerg_Zergling) continue;

                auto burrowPosition = BWAPI::Position(BWAPI::TilePosition(117, 103));
                BWAPI::Unit cannon = nullptr;
                BWAPI::Unit worker = nullptr;

                for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
                {
                    if (enemy->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon) cannon = enemy;
                    if (enemy->getType().isWorker()) worker = enemy;
                }

                // If there is a cannon and worker: attack the worker
                // If there is a cannon: attack it
                // Otherwise: burrow
                if (cannon && worker)
                {
                    if (zergling->isBurrowed())
                    {
                        zergling->unburrow();
                    }
                    else if (zergling->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Unit ||
                             zergling->getLastCommand().getTarget() != worker)
                    {
                        zergling->attack(worker);
                    }
                }
                else if (cannon)
                {
                    if (zergling->isBurrowed())
                    {
                        zergling->unburrow();
                    }
                    else if (zergling->getLastCommand().getType() != BWAPI::UnitCommandTypes::Attack_Unit ||
                             zergling->getLastCommand().getTarget() != cannon)
                    {
                        zergling->attack(cannon);
                    }
                }
                else
                {
                    int dist = zergling->getDistance(burrowPosition);
                    if (dist > 8)
                    {
                        if (zergling->isBurrowed())
                        {
                            zergling->unburrow();
                        }
                        else
                        {
                            zergling->move(burrowPosition);
                        }
                    }
                    else if (!zergling->isBurrowed())
                    {
                        zergling->burrow();
                    }
                }
            }
        }
    };
}

TEST(TakeExpansion, TakesHarassingZerglingExpansion)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new ZerglingHarassModule();
    };
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 98086;
    test.frameLimit = 12000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 12)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 13)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 14)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 15)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(110, 15)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(4, 94)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(4, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(4, 96)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(4, 97)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(4, 98)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(4, 99)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(5, 94)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(5, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(5, 96)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(5, 97)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(5, 98)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(5, 99)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::TilePosition(117, 103)),
    };

    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<TakeExpansionsStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>());
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool won)
    {
        auto base = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 103)));
        EXPECT_EQ(base->owner, BWAPI::Broodwar->self());
        EXPECT_FALSE(base->resourceDepot == nullptr);
    };

    test.run();
}

TEST(TakeExpansion, TakesLurkerExpansion)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 98086;
    test.frameLimit = 12000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 12)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 13)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 14)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(122, 15)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(110, 15)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(110, 16)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Lurker, BWAPI::TilePosition(117, 103)),
    };

    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<TakeExpansionsStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>());
        Strategist::setOpening(openingPlays);
    };

    test.onFrameOpponent = []()
    {
        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker && !unit->isBurrowed())
            {
                unit->burrow();
            }
        }
    };

    test.onEndMine = [](bool won)
    {
        auto base = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 103)));
        EXPECT_EQ(base->owner, BWAPI::Broodwar->self());
        EXPECT_FALSE(base->resourceDepot == nullptr);
    };

    test.run();
}
