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

TEST(OpponentEconomicModel, DTTiming)
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

        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Gateway, 1, 1800);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 1, 2300);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 1, 2775);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, 3450);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 1, 3625);
    };

    test.onFrameMine = []()
    {
        OpponentEconomicModel::update();

        std::cout << "Earliest DT: " << OpponentEconomicModel::earliestUnitProductionFrame(BWAPI::UnitTypes::Protoss_Dark_Templar) << std::endl;
    };

    test.run();
}


TEST(OpponentEconomicModel, ImpliedForge)
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

        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Gateway, 1, 1800);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 1, 2300);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 1, 2775);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 1, 3450);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 1, 3625);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 1, 4400);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 1, 5200);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Photon_Cannon, 1, 6950);
    };

    test.onFrameMine = []()
    {
        OpponentEconomicModel::update();
    };

    test.run();
}

