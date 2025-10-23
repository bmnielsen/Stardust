#pragma once

#include "TilePosition.h"
#include "PositionOnPath.h"
#include "GatherObservations.h"
#include "ReturnObservations.h"

namespace MiningOptimizationTraining
{
    class MapData
    {
    public:
        std::string mapHash;
        std::unordered_map<TilePosition, std::unordered_map<PositionOnPath, GatherObservations>> resourceToGatherRootNodes;
        std::unordered_map<TilePosition, std::unordered_map<PositionOnPath, ReturnObservations>> resourceToReturnRootNodes;

        void clear()
        {
            resourceToGatherRootNodes.clear();
            resourceToReturnRootNodes.clear();
        }
    };
}
