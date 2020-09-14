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

TEST(Targeting, DragoonsVsDragoons)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(38, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(40, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 30))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(38, 33))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(40, 33))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 33))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 33))),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    // In this scenario, we have four vs. four goons squaring off, where the enemy is configured to always attack the closest enemy unit
    // With no intelligent targeting, the goons will just all fight 1v1 and give a poor result
    test.onEndMine = [](bool won)
    {
        // Ensure there are no enemy dragoons remaining
        EXPECT_EQ(0, Units::countEnemy(BWAPI::UnitTypes::Protoss_Dragoon));
        EXPECT_TRUE(Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon) > 0);
    };

    test.run();
}

TEST(Targeting, DragoonsVsZerglingsAllInRange)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 40;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 31))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(42, 33))),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    // In this scenario, we have eight dragoons vs. two zerglings, where one is close and one is further away, but both are in range of all.
    // With no intelligent targeting, the goons will all shoot at the close zergling first, but we want them to split targeting between the two.
    test.onEndMine = [](bool won)
    {
        // Ensure there are no enemy zerglings remaining
        EXPECT_EQ(0, Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling));
    };

    test.run();
}

TEST(Targeting, DragoonsVsZerglingsSplitRange)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 40;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 30))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 32)))
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(42, 33))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(42, 36))),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    // In this scenario, we have eight dragoons vs. two zerglings, where one is in range of all eight goons, and the other is only in range of four.
    // We want them to split targeting so both zerlings are killed with the first volley.
    test.onEndMine = [](bool won)
    {
        // Ensure there are no enemy zerglings remaining
        EXPECT_EQ(0, Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling));
    };

    test.run();
}

TEST(Targeting, MixedVsZerglings)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new AttackNearestUnitModule();
    };
    test.map = Maps::GetOne("Tau Cross");
    test.randomSeed = 42;
    test.frameLimit = 40;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(41, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(42, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(43, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(44, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(BWAPI::TilePosition(45, 31))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(42, 32))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(43, 32))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(42, 33)) + BWAPI::Position(16, -5)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(42, 34))),
    };

    Base *baseToAttack = nullptr;

    // Order them to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(93, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    // In this scenario, we have five dragoons and two zealots vs. two zerglings, where one is in range of the zealots and goons, and the other is
    // only in range of the goons.
    // We want one of the dragoons to target the zergling being attacked by the zealots, and the others to kill the remainin zergling.
    test.onEndMine = [](bool won)
    {
        // Ensure there are no enemy zerglings remaining
        EXPECT_EQ(0, Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling));
    };

    test.run();
}

