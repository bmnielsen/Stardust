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
                ProtossStrategy::kPvP_NZCore,
                ProtossStrategy::kPvP_NZCoreDt,
                ProtossStrategy::kPvP_ZCore,
                ProtossStrategy::kPvP_ZZCore,
                ProtossStrategy::kPvP_ZCoreZ,
                ProtossStrategy::kPvP_1012Gate,
                ProtossStrategy::kPvP_2GateDt,
                ProtossStrategy::kPvP_2GateDtExpo,
                ProtossStrategy::kPvP_2GateReaver,
                ProtossStrategy::kPvP_3GateRobo,
                ProtossStrategy::kPvP_3GateSpeedZeal,
                ProtossStrategy::kPvP_4GateGoon,
                ProtossStrategy::kPvP_12Nexus,
                ProtossStrategy::kPvP_99Gate,
                ProtossStrategy::kPvP_99ProxyGate
        };

        return *randomElement(openings.begin(), openings.end());
    }

    std::string randomZealotOpening()
    {
        std::vector<std::string> openings = {
                ProtossStrategy::kPvP_1012Gate,
                ProtossStrategy::kPvP_3GateSpeedZeal,
                ProtossStrategy::kPvP_99Gate,
                ProtossStrategy::kPvP_99ProxyGate
        };

        return *randomElement(openings.begin(), openings.end());
    }
}

void addBananaBrainModule(BWTest &test)
{
    BananaBrain* bbModule;
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
}

TEST(BananaBrain, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        test.opponentName = "BananaBrain";
        BananaBrain* bbModule;
        test.maps = Maps::Get("cog2022");
        test.opponentRace = BWAPI::Races::Protoss;
        test.frameLimit = 8000;
        test.opponentModule = [&]()
        {
            bbModule = new BananaBrain();
            bbModule->strategyName = randomOpening();
            bbModule->strategyName = randomZealotOpening();
            bbModule->strategyName = randomOpening();
            bbModule->strategyName = ProtossStrategy::kPvP_ZZCore;
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
            if (test.sharedMemory)
            {
                std::string opening = test.sharedMemory;
                std::replace(opening.begin(), opening.end(), '/', '_');
                replayName << "_" << opening;
            }
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
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Protoss;
    test.maps = Maps::Get("sscait");
    test.opponentModule = [&]()
    {
        bbModule = new BananaBrain();
        bbModule->strategyName = randomOpening();
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
        if (test.sharedMemory)
        {
            std::string opening = test.sharedMemory;
            std::replace(opening.begin(), opening.end(), '/', '_');
            replayName << "_" << opening;
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.run();
}

TEST(BananaBrain, 1012Gate)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_3GateSpeedZeal;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, 2GateDT)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.frameLimit = 15000;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_2GateDt;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, BBS)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Outsider");
    test.randomSeed = 37141;
    test.frameLimit = 8000;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = TerranStrategy::kTvP_BBS;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, BenzeneFFE9Gate)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 37141;
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_99Gate;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, BenzeneFFEProxy9Gate)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 37141;
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_99ProxyGate;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, ProxyGates)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.map = Maps::GetOne("NeoHeartbreakerRidge");
    test.randomSeed = 71137;
    test.frameLimit = 8000;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_99ProxyGate;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, 99Gate)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.map = Maps::GetOne("FightingSpirit");
    test.randomSeed = 79647;
    test.frameLimit = 15000;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_99ProxyGate;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, Robo)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.map = Maps::GetOne("Eclipse");
    test.randomSeed = 45417;
    test.frameLimit = 12000;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_3GateRobo;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, 4Gate)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.map = Maps::GetOne("Polypoid");
    test.randomSeed = 1878;
    test.frameLimit = 15000;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_4GateGoon;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, NZCoreDt)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Breakers");
    test.frameLimit = 15000;
    test.randomSeed = 62090;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_NZCoreDt;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, ZZCore)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("MatchPoint");
    test.randomSeed = 11329;
    test.frameLimit = 8000;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_ZZCore;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, Random)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Random;
    test.maps = Maps::Get("cog");
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
//        bbModule->strategyName = ProtossStrategy::kPvP_NZCore;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, NZCore)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Python");
    test.randomSeed = 26255;
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_NZCore;
        return bbModule;
    };

    test.run();
}

TEST(BananaBrain, RunAsBB)
{
    BWTest test;
    test.myRace = BWAPI::Races::Protoss;
    test.opponentRace = BWAPI::Races::Protoss;
    test.myModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_NZCore;
        return bbModule;
    };
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };
    test.onStartOpponent = []()
    {
        Log::SetOutputToConsole(true);
    };
    test.onEndMine = [&](bool won)
    {
        std::ostringstream replayName;
        replayName << "BananaBrain_" << test.map->shortname();
        if (won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.expectWin = false;
    test.run();
}

TEST(BananaBrain, ZCoreZ)
{
    BWTest test;
    test.myRace = BWAPI::Races::Protoss;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("MatchPoint");
    test.randomSeed = 91244;
    test.myModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_NZCore;
        return bbModule;
    };
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };
    test.onStartOpponent = []()
    {
        Log::SetOutputToConsole(true);
    };
    test.onEndMine = [&](bool won)
    {
        std::ostringstream replayName;
        replayName << "BananaBrain_" << test.map->shortname();
        if (won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.expectWin = false;
    test.run();
}
