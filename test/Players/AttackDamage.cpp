#include "BWTest.h"
#include "DoNothingModule.h"

#include "Players.h"

TEST(AttackDamage, VariousUnits)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 10;
    test.expectWin = false;

    // Order the dragoon to attack the bottom base
    test.onStartMine = []()
    {
        EXPECT_EQ(Players::attackDamage(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Zealot,
                                        BWAPI::Broodwar->enemy(),
                                        BWAPI::UnitTypes::Zerg_Zergling), 16);
    };

    test.run();
}
