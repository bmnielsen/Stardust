#pragma once

#include "Noncopyable.h"

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
        std::unordered_map<TilePosition, std::unordered_set<PositionAndVelocity>> resourceToReturnPathStartPositions;

        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            resourceToGatherPaths.clear();
            resourceToReturnPaths.clear();
            resourceToReturnPathStartPositions.clear();
        }

        // Ensure we never copy map data
        [[no_unique_address]] noncopyable _ = {};
    };
}
