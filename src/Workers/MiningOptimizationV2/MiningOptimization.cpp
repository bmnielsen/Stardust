#include "MiningOptimization.h"

#include "DataModel/Serialization.h"
#include "PathOptimizer.h"

namespace MiningOptimization
{
    namespace
    {
        MapData mapData;

        std::unique_ptr<PathOptimizer<GatherArrivalData>> gatherOptimizer;
        std::unique_ptr<PathOptimizer<ReturnArrivalData>> returnOptimizer;
    }

    void initialize()
    {
        Serialization::setGameParameters(BWAPI::Broodwar->mapHash());
        Serialization::readMapData(mapData);

        gatherOptimizer = std::make_unique<PathOptimizer<GatherArrivalData>>(mapData.resourceToGatherPaths,
                                                                             mapData.positionDeltas,
                                                                             mapData.minimumNextPathLength);
        returnOptimizer = std::make_unique<PathOptimizer<ReturnArrivalData>>(mapData.resourceToReturnPaths,
                                                                             mapData.positionDeltas,
                                                                             mapData.minimumNextPathLength);
    }

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        gatherOptimizer->optimize(worker, depot, resource);
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        returnOptimizer->optimize(worker, depot, resource);
    }
}
