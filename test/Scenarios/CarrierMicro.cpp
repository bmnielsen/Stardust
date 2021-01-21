#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Units.h"
#include "Plays/Offensive/AttackIslandExpansion.h"

namespace
{
    class AttackNearestUnitModule : public DoNothingModule
    {
    public:
        void onFrame() override
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->canAttack() || unit->getType().isWorker()) continue;

                int closestDist = INT_MAX;
                BWAPI::Unit closest = nullptr;
                for (auto enemy : BWAPI::Broodwar->enemy()->getUnits())
                {
                    if (!enemy->exists() || !enemy->isVisible()) continue;

                    auto dist = unit->getDistance(enemy);
                    if (dist < closestDist)
                    {
                        closestDist = dist;
                        closest = enemy;
                    }
                }

                if (closest)
                {
                    BWAPI::UnitCommand currentCommand(unit->getLastCommand());
                    if (currentCommand.getType() == BWAPI::UnitCommandTypes::Attack_Unit &&
                        currentCommand.getTarget() == closest)
                    {
                        continue;
                    }

                    unit->attack(closest);
                }
            }
        }
    };
}

TEST(CarrierMicro, CarrierVsIslandBase)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 42;
    test.frameLimit = 7500;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Carrier, BWAPI::TilePosition(47, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(62, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(4, 15)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(5, 15)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(6, 15)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(4, 16)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(5, 16)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(6, 16)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(4, 17)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(5, 17)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(6, 17)),
    };

    test.removeStatic = {
            BWAPI::TilePosition(63, 116) // Blocking neutral at bottom island expo
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::TilePosition(62, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hatchery, BWAPI::TilePosition(62, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(62, 117)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(61, 117)),
    };

    test.onFrameMine = []()
    {
        if (BWAPI::Broodwar->getFrameCount() == 20)
        {
            std::cout << "Carrier range: " << (*Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Carrier).begin())->groundRange() << std::endl;
        }
    };

    test.onEndMine = [](bool win)
    {
        auto base = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(62, 119)));
        if (base)
        {
            EXPECT_EQ(nullptr, base->owner);
        }
    };

    test.run();
}
