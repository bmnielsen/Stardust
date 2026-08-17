#include "BWTest.h"

#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"

TEST(RetreatThroughEnemyArmy, AttacksBlockingArmy)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 1000;

    // We have a few dragoons on their way towards the enemy base
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(91, 75)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(94, 69)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(94, 71)),
    };

    // Enemy has some units defending
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(106, 88)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(106, 89)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hydralisk, BWAPI::TilePosition(106, 90)),
    };

    Base *baseToAttack = nullptr;

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 117)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Spawn an army behind ours
    test.onFrameOpponent = []()
    {
        if (BWAPI::Broodwar->getFrameCount() == 210)
        {
            for (int x = 90; x < 94; x++)
            {
                for (int y = 84; y < 86; y++)
                {
                    BWAPI::Broodwar->createUnit(
                        BWAPI::Broodwar->self(),
                        BWAPI::UnitTypes::Zerg_Hydralisk,
                        BWAPI::Position(BWAPI::TilePosition(x, y)));
                }
            }
        }
    };

    test.run();
}
