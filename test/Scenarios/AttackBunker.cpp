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
        void onFrame() override
        {
            BWAPI::Unit bunker = nullptr;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType() == BWAPI::UnitTypes::Terran_Bunker)
                {
                    bunker = unit;
                    break;
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
                }

                if (unit->getType() == BWAPI::UnitTypes::Terran_SCV &&
                    BWAPI::Broodwar->getFrameCount() % 4 == 0)
                {
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
                    }
                    else
                    {
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
    test.frameLimit = 2000;

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
