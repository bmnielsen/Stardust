#include "BWTest.h"
#include "BananaBrain.h"
#include "DoNothingModule.h"

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

TEST(EarlyGameWorkerScoutTests, MisdetectsProxyRushWithBuildingsAtBackOfBase)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 42;
    test.frameLimit = 5000;
    test.expectWin = false;
    test.opponentModule = [&]()
    {
        return new DoNothingModule();
    };
    test.opponentInitialUnits = {
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(879, 276), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(1100, 231), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::Position(832, 528), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(907, 391), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(907, 402), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(1168, 384), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(1156, 441), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(957, 188), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(907, 160), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(907, 340), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(922, 504), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(910, 276), true),
        UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(956, 234), true),
    };
    test.run();
}
