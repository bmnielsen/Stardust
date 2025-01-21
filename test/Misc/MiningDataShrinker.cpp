#include "gtest/gtest.h"

#include "MiningOptimization/ObservationDataFiles.h"
#include "MiningOptimization/GatherPositionObservations.h"

TEST(MiningDataShrinker, VermeerGather)
{
    Log::initialize();
    Log::SetOutputToConsole(true);

    WorkerMiningOptimization::overrideGameParameters("0a306408d42d64cdef654b36286903b411246714", 3, 12, 5, 0, 0);

    std::map<TilePosition, std::unordered_map<PositionAndVelocity, WorkerMiningOptimization::GatherPositionObservations>>
        resourceToOptimalGatherPositions;

    WorkerMiningOptimization::readGatherPositionObservations(true, resourceToOptimalGatherPositions);

    WorkerMiningOptimization::reduceGatherData(resourceToOptimalGatherPositions);

    WorkerMiningOptimization::writeGatherPositionObservations(true, resourceToOptimalGatherPositions);
}
