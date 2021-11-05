#include "BWTest.h"
#include "Header.h"
#include "StardustAIModule.h"

TEST(McRave, RunOne)
{
    BWTest test;
    McRaveModule* mcraveModule;
    test.opponentName = "McRave";
    test.opponentRace = BWAPI::Races::Zerg;
    test.maps = Maps::Get("sscait");
    test.opponentModule = [&]()
    {
        mcraveModule = new McRaveModule();
        return mcraveModule;
    };
    test.onStartOpponent = [&]()
    {
//        std::cout << "Crona strategy: " << bbModule->strategyName << std::endl;
//        if (test.sharedMemory)
//        {
//            strncpy(test.sharedMemory,
//                    bbModule->strategyName.c_str(),
//                    std::min(255UL, bbModule->strategyName.size()));
//        }

        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [&test](bool won)
    {
        std::ostringstream replayName;
        replayName << "McRave_" << test.map->shortname();
        if (!won)
        {
            replayName << "_LOSS";
        }
//        if (test.sharedMemory) replayName << "_" << test.sharedMemory;
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.run();
}
