#include "MiningOptimization.h"

#include "DataModel/Serialization.h"
#include "PathOptimizer.h"

#include "DebugFlag_MiningOptimization.h"

#if OUTPUT_STATISTICS
#include "PathStatistics.h"
#endif

namespace MiningOptimization
{
    namespace
    {
        MapData mapData;

        std::unique_ptr<PathOptimizer<GatherArrivalData>> gatherOptimizer;
        std::unique_ptr<PathOptimizer<ReturnArrivalData>> returnOptimizer;

#if OUTPUT_STATISTICS
        PathStatistics gatherPathStatistics;
        PathStatistics returnPathStatistics;
#endif
    }

    void initialize()
    {
        Serialization::setGameParameters(BWAPI::Broodwar->mapHash());
        Serialization::readMapData(mapData);

        gatherOptimizer = std::make_unique<PathOptimizer<GatherArrivalData>>(mapData, mapData.resourceToSerializedGatherPaths);
        returnOptimizer = std::make_unique<PathOptimizer<ReturnArrivalData>>(mapData, mapData.resourceToSerializedReturnPaths);

#if OUTPUT_STATISTICS
        gatherPathStatistics.reset();
        returnPathStatistics.reset();
#endif
    }

    void update()
    {
        gatherOptimizer->clearDeadWorkers();
        returnOptimizer->clearDeadWorkers();

#if OUTPUT_STATISTICS
        gatherOptimizer->updateStatistics(gatherPathStatistics);
        returnOptimizer->updateStatistics(returnPathStatistics);
#endif
    }

    void gameEnd()
    {
#if OUTPUT_STATISTICS
        auto outputStatistics = [](const PathStatistics &pathStatistics)
        {
            if (pathStatistics.count == 0) return (std::string)"No Data";

            std::ostringstream out;
            out << std::fixed << std::setprecision(1)
                << pathStatistics.count << " collections, "
                << pathStatistics.withPath << " with path data"
                << " (" << ((double)pathStatistics.withPath * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withPathFollowedToCompletion << " with path followed to completion"
                << " (" << ((double)pathStatistics.withPathFollowedToCompletion * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withExpectedArrivalFrame << " with expected arrival frame"
                << " (" << ((double)pathStatistics.withExpectedArrivalFrame * 100.0 / (double)pathStatistics.count) << "%), "
                << pathStatistics.withExpectedActionFrame << " with expected action frame"
                << " (" << ((double)pathStatistics.withExpectedActionFrame * 100.0 / (double)pathStatistics.count) << "%)";
            if (pathStatistics.withTakeover > 0)
            {
                out << ", " << pathStatistics.withTakeover << " takeover collections, "
                    << pathStatistics.patchSwitches << " with patch switch"
                    << " (" << ((double)pathStatistics.patchSwitches * 100.0 / (double)pathStatistics.withTakeover) << "%), "
                    << pathStatistics.withPlannedPatchLock << " with planned patch lock"
                    << " (" << ((double)pathStatistics.withPlannedPatchLock * 100.0 / (double)pathStatistics.withTakeover) << "%)";
                if (pathStatistics.withPlannedPatchLock > 0)
                {
                    out << ", " << pathStatistics.withExpectedPatchLockFrame << " with expected patch lock frame"
                        << " (" << ((double)pathStatistics.withExpectedPatchLockFrame * 100.0 / (double)pathStatistics.withPlannedPatchLock) << "%)";
                }
            }
            return out.str();
        };
        Log::Get() << "Gather path statistics: " << outputStatistics(gatherPathStatistics);
        Log::Get() << "Return path statistics: " << outputStatistics(returnPathStatistics);
#endif
    }

    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        gatherOptimizer->forWorker(worker, depot, resource).optimize();
    }

    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
    {
        returnOptimizer->forWorker(worker, depot, resource).optimize();
    }
}
