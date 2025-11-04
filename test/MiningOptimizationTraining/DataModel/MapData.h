#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "GatherArrivalData.h"
#include "ReturnArrivalData.h"

namespace MiningOptimizationTraining
{
    class MapData
    {
    public:
        std::string mapHash;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPath>> resourceToGatherPaths;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPath>> resourceToReturnPaths;

        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            resourceToGatherPaths.clear();
            resourceToReturnPaths.clear();
        }
    };
}
