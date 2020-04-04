#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestAttackBasePlay.h"
#include "Units.h"

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

        void onFrame() override
        {
            if (BWAPI::Broodwar->getFrameCount() == attackFrame)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!isCombatUnit(unit->getType())) continue;
                    unit->attack(targetPosition);
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
        return new AttackAtFrameModule(BWAPI::Position(BWAPI::TilePosition(7, 7)), 500);
    };
    test.map = "maps/sscai/(4)La Mancha1.1.scx";
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

    // Order the dragoon to attack the bottom base
    test.onStartMine = [&baseToAttack]()
    {
        baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(9, 118)));

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
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
    test.map = "maps/sscai/(4)La Mancha1.1.scx";
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

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestAttackBasePlay>(baseToAttack));
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
