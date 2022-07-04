#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "OpponentEconomicModel.h"

#include "UnitUtil.h"

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

TEST(OpponentEconomicModel, ImpliedProducers)
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

        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Gateway, 1, 1784);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 2, 2759);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 3, 2945);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 4, 3350);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 5, 4325);
        OpponentEconomicModel::opponentUpgraded(BWAPI::UpgradeTypes::Singularity_Charge, 1, 4650);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 5, 5168);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 6, 5168);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 7, 5920);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 8, 5920);
        OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 9, 6332);
    };

    test.onFrameMine = []()
    {
        OpponentEconomicModel::update();
    };

    test.run();
}

TEST(OpponentEconomicModel, PylonPulling)
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
        if (BWAPI::Broodwar->getFrameCount() == 5)
        {
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Gateway, 151, 1684, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 2, 2839, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 160, 2732, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 168, 3332, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 171, 3626, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 181, 5932, false);
        }

        OpponentEconomicModel::update();
    };

    test.run();
}

