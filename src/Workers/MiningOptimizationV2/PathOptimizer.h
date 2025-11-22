#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"
#include "WorkerPathOptimizer.h"

#include "DebugFlag_MiningOptimization.h"

#if OUTPUT_STATISTICS
#include "PathStatistics.h"
#endif

namespace MiningOptimization
{
    template <typename ObservationType>
    class PathOptimizer
    {
    public:
        PathOptimizer(const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>>> &pathData,
                      const std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
                      const unsigned int minimumNextPathLength)
                : pathData(pathData)
                , positionDeltas(positionDeltas)
                , minimumNextPathLength(minimumNextPathLength)
        {}

        WorkerPathOptimizer<ObservationType> &forWorker(const MyWorker &worker, const MyUnit &depot, const Resource &resource)
        {
            auto it = workers.find(worker);
            if (it == workers.end() || !it->second.matches(depot, resource))
            {
                workers.erase(worker);

                auto patchTile = TilePosition::fromBWAPI(resource->tile);
                auto &patchPathData = pathData.contains(patchTile) ? pathData.at(patchTile) : emptyWorkerPathData;
                auto item = WorkerPathOptimizer<ObservationType>{patchPathData, positionDeltas, minimumNextPathLength, worker, depot, resource};
                it = workers.emplace(worker, std::move(item)).first;
            }
            return it->second;
        }

        void clearDeadWorkers()
        {
            for (auto it = workers.begin(); it != workers.end(); )
            {
                if (it->first->exists())
                {
                    it++;
                }
                else
                {
                    it = workers.erase(it);
                }
            }
        }

#if OUTPUT_STATISTICS
        void updateStatistics(PathStatistics &pathStatistics)
        {
            for (auto &[_, workerPathOptimizer] : workers) workerPathOptimizer.updateStatistics(pathStatistics);
        }
#endif

    private:
        const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>>> &pathData;
        const std::vector<std::pair<int8_t, int8_t>> &positionDeltas;
        const unsigned int minimumNextPathLength;

        std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>> emptyWorkerPathData;

        std::map<MyWorker, WorkerPathOptimizer<ObservationType>> workers;
    };
}
