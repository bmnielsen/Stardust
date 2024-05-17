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

TEST(ForgeExpandWallDefense, 9PoolLings)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::TilePosition(7, 117)), 500);
    };
    test.map = Maps::GetOne("Spirit");
    test.randomSeed = 30841;
    test.frameLimit = 2000;
    test.expectWin = false;

    // Equal dragoon armies at each side of the choke
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(1052, 3552), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::Position(1152, 3552), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::Position(1216, 3440), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(1056, 3520), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::Position(992, 3488), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Forge, BWAPI::Position(1072, 3456), true),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(1216, 3222), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(1232, 3222), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(1200, 3222), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(1216, 3238), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(1232, 3238), true),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(1200, 3238), true),
    };

//    // Order the dragoons to attack the bottom base
//    test.onStartMine = []()
//    {
//        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 118)));
//
//        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());
//
//        std::vector<std::shared_ptr<Play>> openingPlays;
//        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack));
//        Strategist::setOpening(openingPlays);
//    };
//
//    // Order the dragoons to attack the bottom base
//    test.onStartOpponent = []()
//    {
//        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(7, 7)));
//
//        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());
//
//        std::vector<std::shared_ptr<Play>> openingPlays;
//        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
//        Strategist::setOpening(openingPlays);
//    };
//
//    test.onEndMine = [](bool)
//    {
//        // Ensure there are no known enemy combat units
//        bool hasEnemyCombatUnit = false;
//        for (auto &unit : Units::allEnemy())
//        {
//            if (isCombatUnit(unit->type))
//            {
//                hasEnemyCombatUnit = true;
//                break;
//            }
//        }
//
//        EXPECT_FALSE(hasEnemyCombatUnit);
//    };

    test.run();
}
