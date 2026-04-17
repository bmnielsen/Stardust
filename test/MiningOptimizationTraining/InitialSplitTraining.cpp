#include "BWTest.h"

#include "DoNothingModule.h"
#include "DataModel/Serialization.h"
#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"
#include "ClearOpponentUnitsModule.h"

#define VERBOSE_LOGGING false

using namespace MiningOptimizationTraining;

namespace
{
    void initializeOrderProcessTimerResetValues(const Maps::MapMetadata &map)
    {
        std::vector<Maps::MapMetadata> maps = {map};

        InitialWorkerMapData data;
        Serialization::setGameParameters(map.openbwHash);
        Serialization::readMapData(data);
        data.startingWorkerPositionToOrderProcessTimerReset.clear();

        bool opponentIsZerg;

        auto runner = [&](BWTest test)
        {
            test.opponentModule = []()
            {
                return new DoNothingModule();
            };
            test.myModule = []()
            {
                return new DoNothingModule();
            };
            test.allowOpponentOutput = false;
            test.expectWin = false;
            test.writeReplay = false;
            test.frameLimit = 15;
            test.onFrameMine = [&]()
            {
                // The timer changes on the engine's frame 8, but there is misalignment due to how the test hooks are applied so we need to look at
                // frame 10
                if (BWAPI::Broodwar->getFrameCount() != 10) return;

                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!unit->getType().isWorker()) continue;

                    // Get the order process timer and rewind, since the value we read is after the unit's orders have been processed
                    auto orderProcessTimer = unit->getOrderProcessTimer() + 1;
                    if (orderProcessTimer == 9) orderProcessTimer = 0;

                    auto &startingPositionData = data.startingWorkerPositionToOrderProcessTimerReset[unit->getPosition()];
                    bool found = false;
                    for (auto &existingData : startingPositionData)
                    {
                        if (existingData.value != orderProcessTimer) continue;
                        if (existingData.opponentIsZerg != opponentIsZerg) continue;

                        found = true;
                        existingData.opponentStartLocationsCount++;
                    }
                    if (!found)
                    {
                        startingPositionData.emplace_back(orderProcessTimer, opponentIsZerg, 1, test.randomSeed);
                    }
                }
            };
            test.run();
        };

        opponentIsZerg = true;
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Zerg);

        opponentIsZerg = false;
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Protoss);

#if VERBOSE_LOGGING
        for (const auto &[startPosition, observations] : data.startingWorkerPositionToOrderProcessTimerReset)
        {
            std::ostringstream dbg;
            dbg << "\n" << startPosition << ":\n";
            std::string sep;
            for (const auto &observation : observations)
            {
                dbg << sep << " " << observation;
                sep = "\n";
            }
            Log::Get() << dbg.str();
        }
#endif

        Serialization::writeMapData(data);
    }

    void initialPathExploration(const Maps::MapMetadata &map)
    {
        BWTest test;
        test.map = std::make_shared<Maps::MapMetadata>(map);
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        test.myModule = [&]()
        {
            ExploreStartPositionsModuleOptions options;
            options.loadMapData = false;
            return new ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>(options);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.timeLimit = INT_MAX;
        test.run();
    }
}

TEST(InitializeOrderProcessTimerResetValues, Vermeer)
{
    initializeOrderProcessTimerResetValues(*Maps::GetOne("Vermeer"));
}

TEST(InitializeOrderProcessTimerResetValues, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        initializeOrderProcessTimerResetValues(*test.map);
    });
}

TEST(ExploreInitialStartPositions, Vermeer)
{
    initialPathExploration(*Maps::GetOne("Vermeer"));
}
