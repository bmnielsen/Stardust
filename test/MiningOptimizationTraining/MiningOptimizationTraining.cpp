#include "BWTest.h"

#include "MiningOptimizationV2/DataModel/Serialization.h"
#include "Trainers.h"

using namespace MiningOptimizationTraining;

TEST(MiningOptimizationTraining, All)
{
    Maps::RunOnEach(Maps::Get(), [&](BWTest test)
    {
        // If there is a dist version of the data for this map, assume it is completely processed and skip it
        MiningOptimization::Serialization::setGameParameters(test.map->openbwHash);
        MiningOptimization::MapData data;
        MiningOptimization::Serialization::readMapData(data);
        if (!data.positionDeltas.empty())
        {
            std::cout << "Skipping " << test.map->shortname() << " as it already has dist data" << std::endl;
            if (!test.map->hash.empty()) exportDataFile(test.map->openbwHash, test.map->hash);
            return;
        }

        PathExplorationModuleOptions options;
        options.skipIfMapDataExists = true;
        runInitializeMapData(test, options);
        runExplorePaths(test, options);

        runInitializeOrderProcessTimerResetValues(*test.map, true);
        runInitialSplitExploration(*test.map);

        transform(test.map->openbwHash);
        if (!test.map->hash.empty()) exportDataFile(test.map->openbwHash, test.map->hash);
    });
}
