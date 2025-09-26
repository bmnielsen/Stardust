#include "gtest/gtest.h"
#include "Maps.h"

#include "MiningOptimization/ObservationDataFiles.h"
#include "MiningOptimization/GatherPositionObservations.h"
#include "MiningOptimization/ResourceObservations.h"

namespace
{
    void shrink(
            const Maps::MapMetadata &map,
            WorkerMiningOptimization::ObservationDataFiles::GameParameters &parameters)
    {
        parameters.mapHash = map.openbwHash;
        parameters.exportMapHash = map.hash;

        Log::initialize();
        Log::SetOutputToConsole(true);

        Log::Get() << "Starting " << map.shortfilename();

        WorkerMiningOptimization::ObservationDataFiles::overrideGameParameters(parameters);

        std::map<TilePosition, std::unordered_map<PositionAndVelocity, WorkerMiningOptimization::GatherPositionObservations>>
                resourceToOptimalGatherPositions;
        WorkerMiningOptimization::ObservationDataFiles::readGatherPositionObservations(resourceToOptimalGatherPositions);
        WorkerMiningOptimization::ObservationDataFiles::reduceGatherData(resourceToOptimalGatherPositions);
//        WorkerMiningOptimization::ObservationDataFiles::writeGatherPositionObservations(true, resourceToOptimalGatherPositions, true);

        std::map<TilePosition, std::unordered_map<PositionAndVelocity, WorkerMiningOptimization::ReturnPositionObservations>>
                resourceToOptimalReturnPositions;
        WorkerMiningOptimization::ObservationDataFiles::readReturnPositionObservations(true, resourceToOptimalReturnPositions);
        WorkerMiningOptimization::ObservationDataFiles::reduceReturnData(resourceToOptimalReturnPositions);
        WorkerMiningOptimization::ObservationDataFiles::writeReturnPositionObservations(true, resourceToOptimalReturnPositions, true);

        std::map<TilePosition, std::unordered_set<PositionAndVelocity>> resourceTo10DistancePositions;
        WorkerMiningOptimization::ObservationDataFiles::read10DistanceObservations(resourceTo10DistancePositions);
        WorkerMiningOptimization::ObservationDataFiles::write10DistanceObservations(resourceTo10DistancePositions, true);

        std::map<TilePosition, WorkerMiningOptimization::ResourceObservations> resourceToResourceObservations;
        WorkerMiningOptimization::ObservationDataFiles::readResourceObservations(true, resourceToResourceObservations);
        WorkerMiningOptimization::ObservationDataFiles::writeResourceObservations(true, resourceToResourceObservations, true);

        Log::Get() << "Finished " << map.shortfilename();
    }
}

TEST(MiningDataShrinker, Vermeer)
{
    auto parameters = WorkerMiningOptimization::ObservationDataFiles::GameParameters{"", "", 3, 12, 5, 15, 5};

    shrink(*Maps::GetOne("Vermeer"), parameters);
}

TEST(MiningDataShrinker, SSCAIT)
{
    auto parameters = WorkerMiningOptimization::ObservationDataFiles::GameParameters{"", "", 3, 12, 5, 15, 5};

    for (const auto &map : Maps::Get("sscai"))
    {
        shrink(map, parameters);
    }
}
