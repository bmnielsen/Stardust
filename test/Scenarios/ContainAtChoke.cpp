#include "BWTest.h"
#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"
#include "StardustAIModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Units.h"

#include "Plays/MainArmy/DefendMyMain.h"

namespace
{
    bool isCombatUnit(BWAPI::UnitType type)
    {
        return !type.isBuilding() && !type.isWorker();
    }

    class AttackAtFrameModule : public DoNothingModule
    {
        BWAPI::Position targetPosition;
        int attackFrame;

    public:
        AttackAtFrameModule(BWAPI::Position targetPosition, int attackFrame) : targetPosition(targetPosition), attackFrame(attackFrame) {}

        void onStart() override
        {
            CherryVis::initialize();
            Map::initialize();
        }

        void onFrame() override
        {
            if (BWAPI::Broodwar->getFrameCount() < attackFrame) return;

            if (BWAPI::Broodwar->getFrameCount() == attackFrame)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!isCombatUnit(unit->getType())) continue;
                    unit->attack(targetPosition);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() % 6 == 0)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!isCombatUnit(unit->getType())) continue;
                    if (unit->getGroundWeaponCooldown() > 10) continue;

                    auto nextPos = unit->getPosition() + BWAPI::Position((int) (5 * unit->getVelocityX()), (int) (5 * unit->getVelocityY()));
                    auto dist = PathFinding::GetGroundDistance(unit->getPosition(), targetPosition);
                    auto nextDist = PathFinding::GetGroundDistance(nextPos, targetPosition);
                    if (dist != -1 && nextDist > dist)
                    {
                        unit->attack(targetPosition);
                    }
                }
            }
        }
    };
}

TEST(ContainAtChoke, DragoonsVsDragoonsOnBridge)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new StardustAIModule();
        module->frameSkip = 500;
        return module;
    };
    test.map = Maps::GetOne("La Mancha");
    test.randomSeed = 42;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(20, 57)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(21, 57)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(22, 57)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(20, 58)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(21, 58)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(22, 58)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(13, 64)), // For vision on enemy army
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(10, 65)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(11, 66)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(12, 66)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(10, 66)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(11, 67)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(12, 67)),
    };

    Base *baseToAttack = nullptr;

    // Order the dragoons to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Order the dragoons to attack the bottom base
    test.onStartOpponent = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 7)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

TEST(ContainAtChoke, ZealotsAndDragoonsVsZerglingsOnBridge)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::TilePosition(7, 7)), 500);
    };
    test.map = Maps::GetOne("La Mancha");
    test.randomSeed = 42;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(20, 57)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(21, 57)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(22, 57)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(20, 58)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(21, 58)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::WalkPosition(48, 271)), // For vision on enemy army
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(41, 265)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(43, 265)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(45, 265)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(47, 265)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(49, 265)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(51, 265)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(41, 267)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(43, 267)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(45, 267)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(47, 267)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(49, 267)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(51, 267)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(41, 269)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(43, 269)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(45, 269)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(47, 269)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(49, 269)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(51, 269)),
    };

    Base *baseToAttack = nullptr;

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

// 25 lings vs. 8 zealots and 2 goons
TEST(ContainAtChoke, ZealotsAndDragoonsVsZerglingsOnRamp)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::WalkPosition(50, 375)), 200);
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 19968;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(14, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(15, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(16, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(17, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(14, 96)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(15, 96)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(16, 96)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(17, 96)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(12, 93)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(12, 94)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(127, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(128, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(129, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(130, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(131, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(127, 376)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(128, 376)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(129, 376)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(130, 376)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(131, 376)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(127, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(128, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(129, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(130, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(131, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(127, 378)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(128, 378)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(129, 378)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(130, 378)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(131, 378)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(127, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(128, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(129, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(130, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(131, 379)),
    };

    // Order the dragoon to attack the bottom base
    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<DefendMyMain>());
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

// 8 marines vs. 4 zealots
TEST(ContainAtChoke, ZealotsVsMarinesOnRamp)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::WalkPosition(50, 375)), 200);
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 19968;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(14, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(15, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(16, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(17, 95)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(127, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(129, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(131, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(133, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(127, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(129, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(131, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(133, 377)),
    };

    // Order the dragoon to attack the bottom base
    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<DefendMyMain>());
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

// 12 marines vs. 4 zealots and 2 dragoons
TEST(ContainAtChoke, ZealotsAndDragoonsVsMarinesOnRamp)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::WalkPosition(50, 375)), 200);
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 19968;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(14, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(15, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(16, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::TilePosition(17, 95)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(14, 94)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(15, 94)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(127, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(129, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(131, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(133, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(127, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(129, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(131, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(133, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(127, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(129, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(131, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::WalkPosition(133, 379)),
    };

    // Order the dragoon to attack the bottom base
    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<DefendMyMain>());
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}


// 6 dragoons vs. 8 dragoons, our units start on the wrong side of the ramp
TEST(ContainAtChoke, DragoonsVsDragoonsOnRamp)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::WalkPosition(50, 375)), 20);
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 19968;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(28, 98)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(27, 99)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(26, 100)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(25, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(24, 102))
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(127, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(131, 375)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(127, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(131, 377)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(127, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(131, 379)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(135, 374)),
    };

    // Order the dragoon to attack the bottom base
    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<DefendMyMain>());
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

TEST(ContainAtChoke, DragoonsVsDragoonsRetreatThroughBridge)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new StardustAIModule();
        CherryVis::disable();
        module->frameSkip = 500;
        return module;
    };
    test.map = Maps::GetOne("La Mancha");
    test.randomSeed = 42;
    test.frameLimit = 3000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(10, 65)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(11, 66)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(12, 66)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(10, 66)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(11, 67)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(12, 67)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(8, 71)), // For vision on enemy army
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(5, 72)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(6, 72)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(7, 72)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(5, 73)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(6, 73)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(7, 73)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(5, 74)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(6, 74)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(7, 74)),
    };

    Base *baseToAttack = nullptr;

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Order the dragoons to attack the bottom base
    test.onStartOpponent = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 7)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

TEST(ContainAtChoke, DragoonsVsDragoonsRetreatThroughRamp)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new StardustAIModule();
        CherryVis::disable();
        module->frameSkip = 500;
        return module;
    };
    test.map = Maps::GetOne("La Mancha");
    test.randomSeed = 42;
    test.frameLimit = 3000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(9, 30)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(10, 30)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(11, 30)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(9, 31)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(10, 31)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(11, 31)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::TilePosition(22, 38)), // For vision on enemy army
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(23, 37)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(24, 37)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(25, 37)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(23, 38)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(24, 38)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(25, 38)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(23, 39)),
    };

    Base *baseToAttack = nullptr;

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    // Order the dragoons to attack the bottom base
    test.onStartOpponent = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 7)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool)
    {
        // Ensure there are no known enemy combat units
        bool hasEnemyCombatUnit = false;
        for (auto &unit : Units::allEnemy())
        {
            if (isCombatUnit(unit->type))
            {
                hasEnemyCombatUnit = true;
                break;
            }
        }

        EXPECT_FALSE(hasEnemyCombatUnit);
    };

    test.run();
}

TEST(ContainAtChoke, DragoonsVsDragoonsLargeArmy)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new StardustAIModule();
        CherryVis::disable();
        return module;
    };
    test.map = Maps::GetOne("Alchemist");
    test.randomSeed = 42;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(63, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(64, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(65, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(66, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(67, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(68, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(69, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(70, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(71, 110)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(63, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(64, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(65, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(66, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(67, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(68, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(69, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(70, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(71, 111)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(63, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(64, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(65, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(66, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(67, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(68, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(69, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(70, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(71, 112)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(57, 101)), // For vision on enemy army
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(50, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(51, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(52, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(53, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(54, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(55, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(56, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(58, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(59, 101)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(50, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(51, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(52, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(53, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(54, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(55, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(56, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(57, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(58, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(59, 102)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(50, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(51, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(52, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(53, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(54, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(55, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(56, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(57, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(58, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::TilePosition(59, 103)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Observer, BWAPI::TilePosition(67, 109)), // For vision on enemy army
    };

    Base *baseToAttack = nullptr;

    test.onStartMine = [&baseToAttack]()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);

        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(43, 118)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.onStartOpponent = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);

        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 51)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}

TEST(ContainAtChoke, ChokeDetectFailure)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new StardustAIModule();
        CherryVis::disable();
        return module;
    };
    test.map = Maps::GetOne("Breakers");
    test.randomSeed = 62090;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(67, 118), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(63, 122), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(61, 138), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(65, 140), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(64, 129), true),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(35, 117), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(43, 129), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(43, 124), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(39, 122), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Dragoon, BWAPI::WalkPosition(31, 115), true),
    };

    Base *baseToAttack = nullptr;

    test.onStartMine = [&baseToAttack]()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);

        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 9)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
        Strategist::setOpening(openingPlays);
    };

    test.onStartOpponent = []()
    {
        BWAPI::Broodwar->self()->setUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge, 1);

        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(117, 9)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}
