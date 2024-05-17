#include "BWTest.h"
#include "BananaBrain.h"
#include "StardustAIModule.h"


namespace
{
    template<typename It>
    It randomElement(It start, It end)
    {
        std::mt19937 rng((std::random_device()) ());
        std::uniform_int_distribution<> dis(0, std::distance(start, end) - 1);
        std::advance(start, dis(rng));
        return start;
    }

    std::string randomOpening()
    {
        std::vector<std::string> openings = {
                ZergStrategy::kZvP_5Pool,
                ZergStrategy::kZvP_2HatchLing ,
                ZergStrategy::kZvP_3HatchLing ,
                ZergStrategy::kZvP_9HatchLing ,
                ZergStrategy::kZvP_10HatchLing,
                ZergStrategy::kZvP_2HatchMuta ,
                ZergStrategy::kZvP_3HatchMuta ,
                ZergStrategy::kZvP_2HatchHydra,
                ZergStrategy::kZvP_9734,
                ZergStrategy::kZvP_10PoolLurker,
                ZergStrategy::kZvP_3HatchLurker,
                ZergStrategy::kZvP_NeoSauron,
                ZergStrategy::kZvP_4HatchBeforeGas,
                ZergStrategy::kZvP_5HatchBeforeGas,
                ZergStrategy::kZvP_6Hatch
        };

        return *randomElement(openings.begin(), openings.end());
    }
}

TEST(Crona, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        BananaBrain* bbModule;
        test.opponentName = "Crona";
        test.maps = Maps::Get("cog2022");
        test.opponentRace = BWAPI::Races::Zerg;
        test.frameLimit = 10000;
        test.opponentModule = [&]()
        {
            bbModule = new BananaBrain();
            bbModule->strategyName = randomOpening();
            bbModule->strategyName = ZergStrategy::kZvP_2HatchLing;
//            bbModule->strategyName = ZergStrategy::kZvP_5Pool;
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

TEST(Crona, 2HatchMuta)
{
    BWTest test;
    test.opponentName = "Crona";
    test.map = Maps::GetOne("Syl");
    test.randomSeed = 45417;
    test.frameLimit = 20000;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ZergStrategy::kZvP_2HatchMuta;
        return bbModule;
    };

    test.run();
}

TEST(Crona, 5Pool)
{
    BWTest test;
    test.opponentName = "Crona";
    test.map = Maps::GetOne("Heartbreak");
    test.frameLimit = 12000;
    test.opponentRace = BWAPI::Races::Zerg;
    test.randomSeed = 40356;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ZergStrategy::kZvP_5Pool;
        return bbModule;
    };

    test.run();
}

TEST(Crona, 2HatchLing)
{
    BWTest test;
    test.opponentName = "Crona";
    test.map = Maps::GetOne("Spirit");
    test.frameLimit = 6000;
    test.opponentRace = BWAPI::Races::Zerg;
    test.randomSeed = 40356;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ZergStrategy::kZvP_2HatchLing;
        return bbModule;
    };

    test.run();
}

TEST(Crona, 3HatchLing)
{
    BWTest test;
    test.opponentName = "Crona";
    test.map = Maps::GetOne("Spirit");
    test.frameLimit = 6000;
    test.opponentRace = BWAPI::Races::Zerg;
    test.randomSeed = 40356;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ZergStrategy::kZvP_3HatchLing;
        return bbModule;
    };

    test.run();
}
