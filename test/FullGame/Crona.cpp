#include "BWTest.h"
#include "BananaBrain.h"
#include "StardustAIModule.h"

TEST(Crona, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        BananaBrain* bbModule;
        test.opponentName = "Crona";
        test.maps = Maps::Get("cog");
        test.opponentRace = BWAPI::Races::Zerg;
        test.opponentModule = [&]()
        {
            bbModule = new BananaBrain();
            return bbModule;
        };
        test.onStartOpponent = [&]()
        {
            std::cout << "Crona strategy: " << bbModule->strategyName << std::endl;
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory,
                        bbModule->strategyName.c_str(),
                        std::min(255UL, bbModule->strategyName.size()));
            }

            std::cout.setstate(std::ios_base::failbit);
        };
        test.onEndMine = [&](bool won)
        {
            std::ostringstream replayName;
            replayName << "Crona_" << test.map->shortname();
            if (!won)
            {
                replayName << "_LOSS";
                lost++;
            }
            if (test.sharedMemory) replayName << "_" << test.sharedMemory;
            replayName << "_" << test.randomSeed;
            test.replayName = replayName.str();

            count++;
            std::cout << "---------------------------------------------" << std::endl;
            std::cout << "STATUS AFTER " << count << " GAME" << (count == 1 ? "" : "S") << ": "
                      << (count - lost) << " won; " << lost << " lost" << std::endl;
            std::cout << "---------------------------------------------" << std::endl;
        };
        test.expectWin = false;
        test.run();
    }
}

TEST(Crona, RunOne)
{
    BWTest test;
    BananaBrain* bbModule;
    test.opponentName = "Crona";
    test.opponentRace = BWAPI::Races::Zerg;
    test.maps = Maps::Get("sscait");
    test.opponentModule = [&]()
    {
        bbModule = new BananaBrain();
        return bbModule;
    };
    test.onStartOpponent = [&]()
    {
        std::cout << "Crona strategy: " << bbModule->strategyName << std::endl;
        if (test.sharedMemory)
        {
            strncpy(test.sharedMemory,
                    bbModule->strategyName.c_str(),
                    std::min(255UL, bbModule->strategyName.size()));
        }

        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [&test](bool won)
    {
        std::ostringstream replayName;
        replayName << "Crona_" << test.map->shortname();
        if (!won)
        {
            replayName << "_LOSS";
        }
        if (test.sharedMemory) replayName << "_" << test.sharedMemory;
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.run();
}
