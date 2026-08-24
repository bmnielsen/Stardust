#include "Trainers.h"

#include "ClearOpponentUnitsModule.h"
#include "FileTools.h"
#include "MiningOptimizationTraining/DataTransformation/DataTransformer.h"

#define BASE_PATH "/Users/bruce.nielsen/BW/mining-data/"

namespace MiningOptimizationTraining
{
    void runInitializeMapData(BWTest &test, PathExplorationModuleOptions options)
    {
        options.loadInitialWorkerMapData = false;

        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        test.myModule = [&]()
        {
            return new ExploreStartPositionsModule<InitializeStartPosition>(options);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.run();
    }

    void runExplorePaths(BWTest &test, PathExplorationModuleOptions options)
    {
        options.loadInitialWorkerMapData = false;

        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        test.myModule = [&]()
        {
            return new ExploreStartPositionsModule<ExploreStartPosition>(options);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.timeLimit = INT_MAX;
        test.run();
    }

    void runInitializeOrderProcessTimerResetValues(const Maps::MapMetadata &map, bool skipIfMapDataExists)
    {
        InitialWorkerMapData data;
        Serialization::setGameParameters(map.openbwHash);
        Serialization::readMapData(data);
        if (skipIfMapDataExists && !data.startingWorkerPositionToOrderProcessTimerReset.empty()) return;
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
                // frame 9
                if (BWAPI::Broodwar->getFrameCount() != 9) return;

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

    void runInitialSplitExploration(const Maps::MapMetadata &map)
    {
        BWTest test;
        test.map = std::make_shared<Maps::MapMetadata>(map);
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };

        PathExplorationModuleOptions options;
        options.loadMapData = false;
        ExploreStartPositionsModule<ExploreInitialWorkerStartPosition> myModule(options);
        test.myModule = [&]()
        {
            return &myModule;
        };

        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.timeLimit = INT_MAX;
        test.run();
    }

    void transform(const std::string &mapHash)
    {
        Log::initialize();
        Log::SetOutputToConsole(true);

        MapData data;
        InitialWorkerMapData initialWorkerData;

        Serialization::setGameParameters(mapHash);
        std::cout << "Loading path data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(data);
        std::cout << "Loading initial split data for " << mapHash << "..." << std::endl;
        Serialization::readMapData(initialWorkerData);

        FileTools::overrideBasePath(BASE_PATH "openbw-dist/");
        DataTransformer::transform(data, initialWorkerData);
        FileTools::overrideBasePath("");
    }

    void exportDataFile(const std::string &openbwMapHash, const std::string &mapHash)
    {
        std::ostringstream sourceFilenameBuilder;
        sourceFilenameBuilder << "mining-optimization";
        sourceFilenameBuilder << "_" << openbwMapHash;
        FileTools::overrideBasePath(BASE_PATH "openbw-dist/");
        auto sourceFilename = FileTools::getFilePath(sourceFilenameBuilder.str(), "bin.zstd", false);
        FileTools::overrideBasePath("");

        std::ostringstream destinationFilenameBuilder;
        destinationFilenameBuilder << (BASE_PATH "bw-dist/");
        destinationFilenameBuilder << "mining-optimization";
        destinationFilenameBuilder << "_" << mapHash;
        destinationFilenameBuilder << ".bin.zstd";
        auto destinationFilename = destinationFilenameBuilder.str();

        std::filesystem::copy(sourceFilename, destinationFilename, std::filesystem::copy_options::overwrite_existing);
        std::cout << "Copied " << sourceFilename << " to " << destinationFilename << std::endl;
    }
}
