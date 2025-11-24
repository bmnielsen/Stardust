#pragma once

#include "Noncopyable.h"

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "SerializedPath.h"
#include "GatherArrivalData.h"
#include "ReturnArrivalData.h"

namespace MiningOptimization
{
    class MapData
    {
    public:
        std::string mapHash;

        // To save on bits, we store position deltas as indices into this vector
        std::vector<std::pair<int8_t, int8_t>> positionDeltas;

        // To save on bits, we store next path lengths as an increment from the minimum path length for the map
        unsigned int minimumNextPathLength = 0;

        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<GatherArrivalData>>> resourceToSerializedGatherPaths;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ReturnArrivalData>>> resourceToSerializedReturnPaths;

        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            positionDeltas.clear();
            minimumNextPathLength = 0;
            resourceToSerializedGatherPaths.clear();
            resourceToSerializedReturnPaths.clear();
        }

        // Ensure we never copy map data
        [[no_unique_address]] noncopyable _ = {};
    };
}
