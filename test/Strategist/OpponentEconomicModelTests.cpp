#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "OpponentEconomicModel.h"

TEST(OpponentEconomicModel, Initialization)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.myModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Mancha");
    test.frameLimit = 10;
    test.expectWin = false;
    test.writeReplay = false;

    test.onStartMine = []()
    {
        Log::initialize();
        CherryVis::initialize();
        Map::initialize();
        Log::SetDebug(true);

        OpponentEconomicModel::initialize();
    };

    test.onFrameMine = []()
    {
        // Opponent takes gas at 2300
        if (currentFrame == 0)
        {
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 1, 2300);
        }

        OpponentEconomicModel::update();
    };

    test.run();
}

