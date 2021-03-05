#include "BWTest.h"
#include "UAlbertaBotModule.h"
#include "StardustAIModule.h"

#include "DoNothingStrategyEngine.h"
#include "TestMainArmyAttackBasePlay.h"

#include "Map.h"
#include "Strategist.h"

TEST(RushDefense, Steamhammer9PoolSpeed)
{
    BWTest test;
    test.map = Maps::GetOne("Python");
    test.randomSeed = 30841;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "9PoolSpeed";
        return module;
    };
    test.frameLimit = 5000;
    test.expectWin = false;

    // Simulate a short rush distance 4-pool where the zerglings arrive as we have 11 probes
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(46, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(43, 109)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(46, 121)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(50, 121)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(115, 37))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 118))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 118))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 119))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 119))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 121))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 121))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(60, 124))),
    };

    // Enemy zerglings
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Spawning_Pool, BWAPI::Position(BWAPI::TilePosition(113, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(121, 37))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(122, 37))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(121, 38))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(122, 38))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(121, 39))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(122, 39))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(121, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(122, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::Position(BWAPI::TilePosition(121, 41))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::Position(BWAPI::TilePosition(43, 125))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(120, 52))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(119, 55))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(71, 107))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(72, 105))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(72, 104))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(74, 100))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(100, 41))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(101, 41))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(101, 41))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(123, 24))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Zergling, BWAPI::Position(BWAPI::TilePosition(124, 24))),
    };

//    test.onStartMine = []()
//    {
//        std::vector<std::shared_ptr<Play>> openingPlays;
//        openingPlays.emplace_back(std::make_shared<DefendBase>(Map::getMyMain()));
//        Strategist::setOpening(openingPlays);
//    };
//
//    test.onFrameMine = []()
//    {
//        // Quit if we have no probes left
//        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) == 0) BWAPI::Broodwar->leaveGame();
//    };

    test.run();
}

TEST(RushDefense, SteamhammerBBS)
{
    BWTest test;
    test.map = Maps::GetOne("Python");
    test.randomSeed = 30841;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Bio";
        return module;
    };
    test.frameLimit = 5000;
    test.expectWin = false;

    // Simulate a short rush distance 4-pool where the zerglings arrive as we have 11 probes
    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(46, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Pylon, BWAPI::TilePosition(43, 109)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(46, 121)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Gateway, BWAPI::TilePosition(50, 121)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(115, 37))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 117))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 118))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 118))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 119))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 119))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(40, 121))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(41, 121))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(60, 124))),
    };

    // Enemy zerglings
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::Position(BWAPI::TilePosition(113, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::Position(BWAPI::TilePosition(113, 43))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(35, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(121, 37))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(122, 37))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(121, 38))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(122, 38))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(121, 39))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(122, 39))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(121, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(122, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::Position(BWAPI::TilePosition(121, 41))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(BWAPI::TilePosition(109, 40))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::Position(BWAPI::TilePosition(109, 42))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(71, 107))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(72, 104))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(74, 98))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(75, 97))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(116, 61))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Marine, BWAPI::Position(BWAPI::TilePosition(116, 62))),
    };

//    test.onStartMine = []()
//    {
//        std::vector<std::shared_ptr<Play>> openingPlays;
//        openingPlays.emplace_back(std::make_shared<DefendBase>(Map::getMyMain()));
//        Strategist::setOpening(openingPlays);
//    };
//
//    test.onFrameMine = []()
//    {
//        // Quit if we have no probes left
//        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Probe) == 0) BWAPI::Broodwar->leaveGame();
//    };

    test.run();
}

TEST(RushDefense, Zealots)
{
    BWTest test;
    test.map = Maps::GetOne("Roadrunner");
    test.randomSeed = 51163;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        CherryVis::disable();
        return new StardustAIModule();
    };
    test.frameLimit = 5000;
    test.expectWin = false;

    test.myInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 118))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 119))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 120))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 121))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 122))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 123))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Probe, BWAPI::Position(BWAPI::TilePosition(103, 124))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(89, 116))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(90, 117))),
    };

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(85, 114))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(86, 115))),
            UnitTypeAndPosition(BWAPI::UnitTypes::Protoss_Zealot, BWAPI::Position(BWAPI::TilePosition(87, 115))),
    };

    test.onStartOpponent = []()
    {
        auto baseToAttack = Map::baseNear(BWAPI::Position(BWAPI::TilePosition(98, 119)));

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(baseToAttack, true));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}