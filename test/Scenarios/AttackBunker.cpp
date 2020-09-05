#include "BWTest.h"

#include "DoNothingModule.h"
#include "Strategist.h"
#include "Map.h"
#include "Units.h"
#include "Plays/MainArmy/AttackEnemyMain.h"

namespace
{
    // Module that loads marines into a bunker and repairs it
    class BunkerModule : public DoNothingModule
    {
    public:
        BunkerModule(bool upgradeRange = false) : upgradeRange(upgradeRange)
        {
            gasMiner = nullptr;
        }

    private:
        bool upgradeRange;
        BWAPI::Unit gasMiner;

        void onFrame() override
        {
            BWAPI::Unit bunker = nullptr;
            BWAPI::Unit refinery = nullptr;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker)
                {
                    bunker = unit;
                }
                if (unit->getType() == BWAPI::UnitTypes::Terran_Refinery)
                {
                    refinery = unit;
                }
            }

            if (!bunker) return;

            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Terran_Marine)
                {
                    if (!unit->isLoaded())
                    {
                        unit->load(bunker);
                    }
                    continue;
                }

                if (BWAPI::Broodwar->getFrameCount() % 4 != 0) continue;

                if (unit->getType() == BWAPI::UnitTypes::Terran_Academy && !unit->isUpgrading())
                {
                    unit->upgrade(BWAPI::UpgradeTypes::U_238_Shells);
                    continue;
                }

                if (unit->getType() != BWAPI::UnitTypes::Terran_SCV) continue;

                int dist = unit->getDistance(bunker);
                if (dist < 200)
                {
                    if (bunker->getHitPoints() < bunker->getType().maxHitPoints())
                    {
                        if (unit->getOrder() != BWAPI::Orders::Repair)
                        {
                            unit->repair(bunker);
                        }
                    }
                    else
                    {
                        unit->move(bunker->getPosition());
                    }

                    continue;
                }

                if (upgradeRange && !gasMiner)
                {
                    gasMiner = unit;
                }

                if (unit == gasMiner)
                {
                    if (!refinery)
                    {
                        unit->build(BWAPI::UnitTypes::Terran_Refinery, BWAPI::TilePosition(86, 118));
                    }
                    else if (refinery->isCompleted() &&
                        unit->getOrder() != BWAPI::Orders::MoveToGas &&
                        unit->getOrder() != BWAPI::Orders::HarvestGas &&
                        unit->getOrder() != BWAPI::Orders::ReturnGas &&
                        unit->getOrder() != BWAPI::Orders::WaitForGas)
                    {
                        unit->rightClick(refinery);
                    }

                    continue;
                }

                if (unit->getOrder() != BWAPI::Orders::MoveToMinerals &&
                    unit->getOrder() != BWAPI::Orders::MiningMinerals &&
                    unit->getOrder() != BWAPI::Orders::ReturnMinerals &&
                    unit->getOrder() != BWAPI::Orders::WaitForMinerals)
                {
                    int closestDist = INT_MAX;
                    BWAPI::Unit closestMineral = nullptr;
                    for (auto minerals : BWAPI::Broodwar->getStaticMinerals())
                    {
                        int mineralDist = unit->getDistance(minerals);
                        if (mineralDist < closestDist)
                        {
                            closestDist = mineralDist;
                            closestMineral = minerals;
                        }
                    }

                    if (closestMineral)
                    {
                        unit->gather(closestMineral);
                    }
                }
            }
        }
    };

    class UpgradeGoonRangeStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            auto upgradeType = BWAPI::UpgradeTypes::Singularity_Charge;

            if (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0) return;
            if (Units::isBeingUpgraded(upgradeType)) return;

            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(UpgradeProductionGoal(upgradeType));
        }
    };

}

TEST(AttackBunker, FourGoons)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new BunkerModule();
    };
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 11))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 12))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 11))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 12))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(113, 12)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Cybernetics_Core, BWAPI::TilePosition(112, 10)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Position(BWAPI::TilePosition(50, 105))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(50, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(51, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(52, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(53, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(50, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(51, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(52, 110))),
    };

    test.onStartMine = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(116, 8)));

        Strategist::setStrategyEngine(std::make_unique<UpgradeGoonRangeStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackEnemyMain>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}

TEST(AttackBunker, FourGoons_EnemyGetsRange)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new BunkerModule(true);
    };
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 11))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 12))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 11))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 12))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(113, 12)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Cybernetics_Core, BWAPI::TilePosition(112, 10)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Position(BWAPI::TilePosition(50, 105))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(50, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(51, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(52, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(53, 109))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(50, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(51, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(52, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(97, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(97, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(97, 118))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(97, 119))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(97, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(97, 121))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::Position(BWAPI::TilePosition(97, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Engineering_Bay, BWAPI::Position(BWAPI::TilePosition(92, 110))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Academy, BWAPI::Position(BWAPI::TilePosition(97, 106))),
    };

    test.onStartMine = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(116, 8)));

        Strategist::setStrategyEngine(std::make_unique<UpgradeGoonRangeStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackEnemyMain>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}


TEST(AttackBunker, ManyGoonsNarrowSpace)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new BunkerModule();
    };
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 65145;
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 11))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 12))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 11))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(123, 12))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 10)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(115, 8)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(113, 12)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Cybernetics_Core, BWAPI::TilePosition(112, 10)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Bunker, BWAPI::Position(BWAPI::TilePosition(72, 113))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(73, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(74, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(73, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(74, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(73, 112))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(74, 112))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(75, 113))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(75, 114))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(75, 115))),
    };

    test.onStartMine = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(116, 8)));

        Strategist::setStrategyEngine(std::make_unique<UpgradeGoonRangeStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<AttackEnemyMain>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}
