#include "BWTest.h"
#include "DoNothingModule.h"
#include "ClearOpponentUnitsModule.h"

namespace
{
    void run(BWTest &test, bool randomizeOrderProcessTimer)
    {
        test.randomSeed = 42;
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.allowOpponentOutput = true;
        test.myRace = BWAPI::Races::Terran;
        test.myModule = [&]()
        {
            return new ClearOpponentUnitsModule(randomizeOrderProcessTimer);
        };
        test.frameLimit = 5000;
        test.expectWin = false;

        test.onEndMine = [&](bool)
        {
            std::map<BWAPI::UnitType, int> unitCounts;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                unitCounts[unit->getType()]++;

                if (unit->getType().isResourceDepot())
                {
                    EXPECT_TRUE(unit->isFlying()) << "Depot should be flying";
                }
            }
            EXPECT_EQ(0, unitCounts[BWAPI::UnitTypes::Terran_SCV]) << "Expect 0 workers";

            if (randomizeOrderProcessTimer)
            {
                EXPECT_EQ(7, unitCounts[BWAPI::UnitTypes::Terran_Bunker]) << "Expect 7 bunkers";
                EXPECT_EQ(7, unitCounts[BWAPI::UnitTypes::Terran_Marine]) << "Expect 7 marines";
            }
        };

        test.run();
    }
}

TEST(ClearOpponentUnitsModuleTest, VermeerNoRandomization)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, false);
}

TEST(ClearOpponentUnitsModuleTest, VermeerRandomization)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, true);
}

TEST(ClearOpponentUnitsModuleTest, BeltwayRandomization)
{
    BWTest test;
    test.map = Maps::GetOne("aiide2023/(4)Beltway_1.1_KeSPA");
    run(test, true);
}

TEST(ClearOpponentUnitsModuleTest, AllRandomization)
{
    Maps::RunOnEach(Maps::Get(""), [](BWTest test)
    {
        run(test, true);
    });
}
