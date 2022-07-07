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

TEST(OpponentEconomicModel, LateObservedGateway)
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
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Gateway, 151, 1685, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 79, 2838, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 160, 2732, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Cybernetics_Core, 167, 3338, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 171, 3718, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 181, 4391, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Photon_Cannon, 207, 6663, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 188, 5892, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 201, 6171, false);
            OpponentEconomicModel::opponentUpgraded(BWAPI::UpgradeTypes::Singularity_Charge, 1, 4782);
            OpponentEconomicModel::opponentUnitDestroyed(BWAPI::UnitTypes::Protoss_Dragoon, 188, 7686);
            OpponentEconomicModel::opponentUnitDestroyed(BWAPI::UnitTypes::Protoss_Dragoon, 201, 7732);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 209, 7774, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Gateway, 202, 7671, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 223, 7898, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 219, 7899, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 216, 7886, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 212, 7913, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 220, 7915, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 208, 7916, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 226, 7943, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 230, 8874, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 243, 9499, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 249, 11126, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 265, 11366, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 228, 11821, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 234, 12023, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 261, 12088, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 273, 12626, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 281, 12763, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 258, 13023, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 256, 13034, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 241, 13413, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 302, 13537, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 300, 13551, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 239, 13438, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 293, 13555, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 298, 13587, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Dragoon, 232, 13496, false);
            OpponentEconomicModel::opponentUnitDestroyed(BWAPI::UnitTypes::Protoss_Zealot, 302, 14309);
            OpponentEconomicModel::opponentUnitDestroyed(BWAPI::UnitTypes::Protoss_Zealot, 281, 14337);
        }

        OpponentEconomicModel::update();

        if (BWAPI::Broodwar->getFrameCount() == 8)
        {
            auto zealots = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot, 15000);
            auto dragoons = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Dragoon, 15000);
            std::cout << "Zealots: [" << zealots.first << "," << zealots.second << "]" << std::endl;
            std::cout << "Dragoons: [" << dragoons.first << "," << dragoons.second << "]" << std::endl;
        }
    };

    test.run();
}

TEST(OpponentEconomicModel, ProxyGates)
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
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Assimilator, 95, 4233, true);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 156, 2428, false);
            OpponentEconomicModel::opponentUnitCreated(BWAPI::UnitTypes::Protoss_Zealot, 151, 2416, false);
        }

        OpponentEconomicModel::update();

        if (BWAPI::Broodwar->getFrameCount() == 8)
        {
            auto zealots = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Zealot, 15000);
            auto dragoons = OpponentEconomicModel::worstCaseUnitCount(BWAPI::UnitTypes::Protoss_Dragoon, 15000);
            std::cout << "Zealots: [" << zealots.first << "," << zealots.second << "]" << std::endl;
            std::cout << "Dragoons: [" << dragoons.first << "," << dragoons.second << "]" << std::endl;
        }
    };

    test.run();
}

