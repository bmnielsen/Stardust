#include "BWTest.h"

#include "DoNothingModule.h"
#include "Strategist.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Plays/Defensive/DefendBase.h"

// Define a module that attacks the mineral line with zerglings
namespace
{
    class AttackMineralLineModule : public DoNothingModule
    {
        void onFrame() override
        {
            if (BWAPI::Broodwar->getFrameCount() < 3) return;

            bool anyCombatUnits = false;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->exists()) continue;
                if (!UnitUtil::IsCombatUnit(unit->getType()) || unit->getType().groundWeapon() == BWAPI::WeaponTypes::None) continue;

                anyCombatUnits = true;

                if (BWAPI::Broodwar->getFrameCount() % 3 != 0) continue;

                BWAPI::Unit closestProbe = nullptr;
                int closestProbeDist = INT_MAX;
                for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
                {
                    if (enemy->getType() != BWAPI::UnitTypes::Protoss_Probe) continue;

                    int dist = unit->getDistance(enemy);
                    if (dist < closestProbeDist)
                    {
                        closestProbeDist = dist;
                        closestProbe = enemy;
                    }
                }

                if (closestProbe)
                {
                    unit->attack(closestProbe);
                }
                else
                {
                    unit->move(BWAPI::Position(BWAPI::TilePosition(122, 8)));
                }
            }

            if (!anyCombatUnits)
            {
                BWAPI::Broodwar->leaveGame();
                std::cout << "No combat units; giving up" << std::endl;
            }
        }
    };
}

TEST(DefendBase, WorkerDefenseZerglings)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackMineralLineModule();
    };
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = "maps/sscai/(4)Fighting Spirit.scx";
    test.frameLimit = 2000;

    // Simulate a short rush distance 4-pool where the zerglings arrive as we have 11 probes
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(118, 11)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 8))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 7))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 6))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 5))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 11)))
    };

    // Enemy zerglings
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(103, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(103, 9)) + BWAPI::Position(20, 0)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(103, 9)) + BWAPI::Position(20, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(103, 9)) + BWAPI::Position(0, 20)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(103, 9)) + BWAPI::Position(-20, 0)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(103, 9)) + BWAPI::Position(0, -20))
    };

    test.onStartMine = []()
    {
        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<DefendBase>(Map::getMyMain()));
        Strategist::setOpening(openingPlays);
    };

    test.onFrameMine = []()
    {
        // Quit if we have no probes left
        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) == 0) BWAPI::Broodwar->leaveGame();
    };

    test.run();
}

TEST(DefendBase, WorkerDefenseZealots)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackMineralLineModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = "maps/sscai/(4)Fighting Spirit.scx";
    test.frameLimit = 2000;

    // Simulate a short rush distance 4-pool where the zerglings arrive as we have 11 probes
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(118, 11)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 8))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 7))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 6))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 10))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 5))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(122, 11)))
    };

    // Enemy zealots
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(103, 9))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(103, 9)) + BWAPI::Position(28, 0))
    };

    test.onStartMine = []()
    {
        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<DefendBase>(Map::getMyMain()));
        Strategist::setOpening(openingPlays);
    };

    test.onFrameMine = []()
    {
        // Quit if we have no probes left
        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) == 0) BWAPI::Broodwar->leaveGame();
    };

    test.run();
}
