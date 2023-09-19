#include "BWTest.h"

#include "DoNothingModule.h"
#include <unistd.h>
#include "Common.h"

TEST(MemoryLeaks, Instrumentation)
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
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 20000;
    test.expectWin = false;

    test.onStartMine = []()
    {
        Log::initialize();
        CherryVis::initialize();
    };

    test.onFrameMine = []()
    {
//        std::vector<std::string> values;
//        for (int i = 0; i < 15; i++)
//        {
//            values.push_back((std::ostringstream() << i).str());
//        }
//
//        CherryVis::setBoardListValue("test", values);
//
//        CherryVis::frameEnd(currentFrame);
        currentFrame++;

        if (currentFrame % 1000 == 0)
        {
            Log::Get() << currentFrame;
            usleep(1000000);
        }
    };

    test.run();
}
