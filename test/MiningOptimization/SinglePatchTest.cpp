#include "BWTest.h"
#include "DoNothingModule.h"
#include "ClearOpponentUnitsModule.h"
#include "DoNothingStrategyEngine.h"
#include "StardustAIModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "WorkerMiningInstrumentation.h"
#include "Units.h"
#include "Workers.h"

#include <algorithm>
#include <random>

TEST(SinglePatchTest, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.randomSeed = 42;
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        return new ClearOpponentUnitsModule();
    };
    test.myModule = []()
    {
        auto module = new StardustAIModule();
        module->enableFrameLimit = false;
        return module;
    };
    test.frameLimit = 10000;
    test.expectWin = false;

    test.onStartMine = []()
    {
        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        // Add a dummy main army play since one is needed
        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(Map::getMyMain()));
        Strategist::setOpening(openingPlays);
    };

    int lastMinerals = 0;
    int lastMineralFrame = -1;
    test.onFrameMine = [&]()
    {
        if (BWAPI::Broodwar->getFrameCount() == 0)
        {
            Resource resource;
            for (auto &candidate : Map::getMyMain()->mineralPatches())
            {
                if (candidate->tile == BWAPI::TilePosition(1, 9))
                {
                    resource = candidate;
                    break;
                }
            }

            int i = 0;
            for (auto &unit : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
            {
                if (i < 2)
                {
                    BWAPI::Broodwar->killUnit(unit->bwapiUnit);
                }
                else
                {
                    Workers::setWorkerMineralPatch(std::static_pointer_cast<MyWorkerImpl>(unit),
                                                   resource,
                                                   Map::getMyMain());

                }
                i++;
            }
        }
        else
        {
            if (BWAPI::Broodwar->self()->gatheredMinerals() > lastMinerals)
            {
                lastMinerals = BWAPI::Broodwar->self()->gatheredMinerals();
                if (lastMineralFrame != -1)
                {
                    Log::Get() << "Minerals returned after " << (BWAPI::Broodwar->getFrameCount() - lastMineralFrame)
                               << ", total mined " << lastMinerals;
                }
                else
                {
                    Log::Get() << "Minerals returned after " << BWAPI::Broodwar->getFrameCount() << ", total mined " << lastMinerals;
                }
                lastMineralFrame = BWAPI::Broodwar->getFrameCount();
            }
        }
    };
    test.run();
}
