#include "gtest/gtest.h"

#include "MiningOptimization/ObservationDataFiles.h"
#include "MiningOptimization/GatherPositionObservations.h"

TEST(MiningDataShrinker, VermeerGather)
{
    Log::initialize();
    Log::SetOutputToConsole(true);

    WorkerMiningOptimization::ObservationDataFiles::overrideGameParameters(
            WorkerMiningOptimization::ObservationDataFiles::GameParameters{"0a306408d42d64cdef654b36286903b411246714", 3, 12, 5, 0, 0}
    );

    std::map<TilePosition, std::unordered_map<PositionAndVelocity, WorkerMiningOptimization::GatherPositionObservations>>
        resourceToOptimalGatherPositions;

    WorkerMiningOptimization::ObservationDataFiles::readGatherPositionObservations(true, resourceToOptimalGatherPositions);

    WorkerMiningOptimization::ObservationDataFiles::reduceGatherData(resourceToOptimalGatherPositions);

    WorkerMiningOptimization::ObservationDataFiles::writeGatherPositionObservations(true, resourceToOptimalGatherPositions, true);
}
