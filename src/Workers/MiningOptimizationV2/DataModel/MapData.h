#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "GatherArrivalData.h"
#include "ReturnArrivalData.h"

namespace MiningOptimizationV2
{
    class MapData
    {
    public:
        std::string mapHash;

        // To save on bits, we store position deltas as indices into this vector
        std::vector<std::pair<uint8_t, uint8_t>> positionDeltas;

        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, GatherPath>> resourceToGatherPaths;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPath>> resourceToReturnPaths;

        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            positionDeltas.clear();
            resourceToGatherPaths.clear();
            resourceToReturnPaths.clear();
        }
    };
}
