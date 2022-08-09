#include "BWTest.h"
#include "DoNothingModule.h"

namespace
{
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
                    unit->attack(targetPosition);
                }
            }
        }
    };
}

TEST(Cannons, MineralLineDefense)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new AttackAtFrameModule(BWAPI::Position(36*8, 30*8), 50);
    };
    test.map = Maps::GetOne("Spirit");
    test.randomSeed = 4349;
    test.frameLimit = 2000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(11, 6)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::TilePosition(10, 9)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::TilePosition(5, 4)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Photon_Cannon, BWAPI::TilePosition(6, 9)),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(27, 80)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(29, 81)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(28, 83)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(26, 83)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(26, 85)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(28, 86)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(27, 88)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::WalkPosition(24, 88)),
    };

    test.run();
}
