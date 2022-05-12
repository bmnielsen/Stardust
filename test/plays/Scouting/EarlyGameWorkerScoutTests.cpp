#include "BWTest.h"
#include "BananaBrain.h"

TEST(EarlyGameWorkerScoutTests, MonitorsEnemyChoke)
{
    BWTest test;
    BananaBrain *bbModule;
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Glaive");
    test.randomSeed = 42;
    test.frameLimit = 6000;
    test.expectWin = false;
    test.opponentModule = [&]()
    {
        bbModule = new BananaBrain();
        bbModule->strategyName = ZergStrategy::kZvP_2HatchLing;
        return bbModule;
    };
    test.onStartOpponent = []()
    {
        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [](bool won)
    {
        // Verify the scout is still doing something
//        EXPECT_NE(Strategist::getWorkerScoutStatus(), Strategist::WorkerScoutStatus::ScoutingFailed);
//        EXPECT_NE(Strategist::getWorkerScoutStatus(), Strategist::WorkerScoutStatus::ScoutingCompleted);
//        EXPECT_NE(Strategist::getWorkerScoutStatus(), Strategist::WorkerScoutStatus::ScoutingBlocked);
    };
    test.run();
}

TEST(EarlyGameWorkerScoutTests, SurvivabilityVsProtossRush)
{
    BWTest test;
    BananaBrain *bbModule;
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Glaive");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;
    test.opponentModule = [&]()
    {
        bbModule = new BananaBrain();
        bbModule->strategyName = ProtossStrategy::kPvP_99Gate;
        return bbModule;
    };
    test.onStartOpponent = []()
    {
        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [](bool won)
    {
        // Verify the scout is still doing something
//        EXPECT_NE(Strategist::getWorkerScoutStatus(), Strategist::WorkerScoutStatus::ScoutingFailed);
//        EXPECT_NE(Strategist::getWorkerScoutStatus(), Strategist::WorkerScoutStatus::ScoutingCompleted);
//        EXPECT_NE(Strategist::getWorkerScoutStatus(), Strategist::WorkerScoutStatus::ScoutingBlocked);
    };
    test.run();
}
