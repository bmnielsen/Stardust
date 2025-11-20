#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"

namespace MiningOptimization
{
    template <typename ObservationType>
    class PathOptimizer
    {
    public:
        PathOptimizer(const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<ObservationType>>> &pathData,
                      const std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
                      const unsigned int minimumNextPathLength)
                : pathData(pathData)
                , positionDeltas(positionDeltas)
                , minimumNextPathLength(minimumNextPathLength)
        {}

        void optimize(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    private:
        const std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, Path<ObservationType>>> &pathData;
        const std::vector<std::pair<int8_t, int8_t>> &positionDeltas;
        const unsigned int minimumNextPathLength;
    };
}
