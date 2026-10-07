#include "BWTest.h"
#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

#include "Map.h"
#include "Strategist.h"
#include "Units.h"
#include "TestAttackBasePlay.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/MainArmy/ForgeFastExpand.h"

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
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
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

TEST(Targeting, CannonsVsZealots)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Polypoid");
    test.randomSeed = 42;
    test.frameLimit = 500;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(3552, 1130), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(3648, 1216), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Nexus, BWAPI::Position(3840, 1168), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(3584, 1280), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(3648, 1280), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(3552, 1200), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Forge, BWAPI::Position(3504, 1280), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(3712, 1216), true),
        };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(89, 39))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(87, 39))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(85, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(83, 40))),
    };

    test.onStartMine = []()
    {
        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<ForgeFastExpand>());
        Strategist::setOpening(openingPlays);
    };

    test.onFrameOpponent = []()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Protoss_Zealot) continue;
            if (unit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Move) continue;
            unit->move(BWAPI::Position(BWAPI::TilePosition(118, 14)));
        }
    };

    test.run();
}

TEST(Targeting, WalledRamp)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Pamir");
    test.randomSeed = 42;
    test.frameLimit = 500;
    test.expectWin = false;

    test.myInitialUnits = {
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(972, 3735), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1083, 3665), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1101, 3730), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1004, 3735), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1120, 3606), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(999, 3767), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(977, 3800), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(908, 3699), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1138, 3753), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1036, 3735), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1075, 3697), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1049, 3618), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1078, 3773), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1031, 3767), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1068, 3740), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1110, 3804), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(927, 3767), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1009, 3671), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1045, 3801), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(977, 3671), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1039, 3703), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1041, 3671), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter, BWAPI::Position(894, 3577), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1017, 3637), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1126, 3639), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(979, 3607), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1085, 3633), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(953, 3639), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(876, 3695), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1066, 3580), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(940, 3703), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(876, 3728), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(914, 3502), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(957, 3575), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(975, 3703), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1007, 3703), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(940, 3735), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(947, 3607), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(985, 3639), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1141, 3720), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(964, 3768), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(930, 3577), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(1031, 3589), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(928, 3536), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(945, 3671), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(854, 3525), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(985, 3573), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(927, 3558), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(1008, 3589), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(1144, 3573), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(907, 3579), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(1007, 3608), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::Position(892, 3536), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(901, 3558), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Arbiter, BWAPI::Position(1011, 3685), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Shuttle, BWAPI::Position(998, 3645), true),
    };

    test.opponentInitialUnits = {
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(600, 3461), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Goliath, BWAPI::Position(565, 3461), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(720, 3264), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(560, 3808), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(560, 3744), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::Position(768, 3536), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(848, 3456), true),
    };

    test.onStartMine = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}
