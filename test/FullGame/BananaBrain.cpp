#include "BWTest.h"
#include "BananaBrain.h"
#include "StardustAIModule.h"

TEST(BananaBrain, RunForever)
{
    int count = 0;
    int lost = 0;
    while (count < 40)
    {
        BWTest test;
        BananaBrain* bbModule;
        test.maps = Maps::Get("aiide");
        test.opponentRace = BWAPI::Races::Protoss;
        test.opponentModule = [&]()
        {
            bbModule = new BananaBrain();
            return bbModule;
        };
        test.onStartOpponent = [&]()
        {
            std::cout << "BananaBrain strategy: " << bbModule->strategyName << std::endl;
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
            replayName << "BananaBrain_" << test.map->shortname();
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

TEST(BananaBrain, RunOne)
{
    BWTest test;
    BananaBrain* bbModule;
    test.opponentRace = BWAPI::Races::Protoss;
    test.maps = Maps::Get("aiide");
    test.opponentModule = [&]()
    {
        bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_4GateGoon;
        return bbModule;
    };
    test.onStartOpponent = [&]()
    {
        std::cout << "BananaBrain strategy: " << bbModule->strategyName << std::endl;
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
        replayName << "BananaBrain_" << test.map->shortname();
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

TEST(BananaBrain, BBS)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = TerranStrategy::kTvP_BBS;
        return bbModule;
    };

    test.run();
}
