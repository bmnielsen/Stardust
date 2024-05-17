#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "Units.h"
#include "TestAttackBasePlay.h"

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

TEST(UnitAttackTimings, UnitAttackTimings)
{
    BWTest test;
    test.myModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 100;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(42, 30)))
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 31)))
    };

    bool onCooldown = false;
    bool bulletCreated = false;
    bool damageDealt = false;

    test.onFrameMine = [&]()
    {
        if (!onCooldown)
        {
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getGroundWeaponCooldown() > 0)
                {
                    std::cout << BWAPI::Broodwar->getFrameCount() << ": cooldown" << std::endl;
                    onCooldown = true;
                }
            }
        }

        if (!bulletCreated)
        {
            for (auto bullet : BWAPI::Broodwar->getBullets())
            {
                if (bullet->getSource() && bullet->getSource()->getPlayer() == BWAPI::Broodwar->self())
                {
                    std::cout << BWAPI::Broodwar->getFrameCount() << ": bullet" << std::endl;
                    bulletCreated = true;
                }
            }
        }

        if (!damageDealt)
        {
            for (auto unit : BWAPI::Broodwar->enemy()->getUnits())
            {
                if (unit->getShields() < unit->getType().maxShields())
                {
                    std::cout << BWAPI::Broodwar->getFrameCount() << ": damage dealt" << std::endl;
                    damageDealt = true;
                }
            }
        }
    };

    test.run();
}
