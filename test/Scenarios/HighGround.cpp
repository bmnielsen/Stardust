#include "BWTest.h"
#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

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

// We are outnumbered, but can retreat to high ground and win.
TEST(HighGround, DragoonsVsDragoons)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.map = Maps::GetOne("Eddy");
    test.randomSeed = 2396;
    test.frameLimit = 2000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(93, 74))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(94, 74))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(95, 74))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(96, 74))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(93, 67))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(94, 67))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(95, 67))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(96, 67))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(97, 67))),
    };

    Base *baseToAttack = nullptr;

    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 14)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool won)
    {
        // Ensure there are no enemy dragoons remaining
        EXPECT_EQ(0, Units::countEnemy(BWAPI::UnitTypes::Protoss_Dragoon));
        EXPECT_TRUE(Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon) > 0);
    };

    test.run();
}
