#pragma once

#include "Noncopyable.h"

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "SerializedPath.h"
#include "GatherArrivalData.h"
#include "ReturnArrivalData.h"

#define OCCURRENCE_SCALE 127

namespace MiningOptimization
{
    struct InitialSplitRotation
    {
        std::set<uint16_t> resendFrames;
        uint16_t gatherArrivalFrame;
        uint16_t gatherActionFrame;
        uint16_t returnArrivalFrame;
        std::set<uint16_t> returnActionFrames;
    };

    struct InitialSplitData
    {
        InitialSplitRotation firstRotation;
        std::map<uint16_t, InitialSplitRotation> firstRotationDeliveryToSecondRotation;
    };

    class MapData
    {
    public:
        std::string mapHash;

        // To save on bits, we store position deltas as indices into this vector
        // The vector is guaranteed to have a maximum size of 128
        // The zero element (delta of 0,0) is always at index 0 in the vector
        std::vector<std::pair<int8_t, int8_t>> positionDeltas;

        // To save on bits, we store the ten distance and resend always arrives deltas as indices into this vector
        // Both could be packed as independent 4-bit numbers into one byte, but testing has shown there is a weak correlation between the two values,
        // allowing us to pack more data into one byte by treating them together
        std::vector<std::pair<int8_t, int8_t>> tenDistanceAndResendAlwaysArrives;

        // To save on bits, we store next path lengths as an increment from the minimum path length for the map
        unsigned int minimumNextPathLength = 0;

        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<GatherArrivalData>>> resourceToSerializedGatherPaths;
        std::unordered_map<TilePosition, std::unordered_map<PositionAndVelocity, SerializedPath<ReturnArrivalData>>> resourceToSerializedReturnPaths;

        std::unordered_map<PositionAndVelocity, std::map<std::pair<TilePosition, TilePosition>, InitialSplitData>>
            startLocationToPatchPairToInitialSplitDataZerg;
        std::unordered_map<PositionAndVelocity, std::map<std::pair<TilePosition, TilePosition>, InitialSplitData>>
            startLocationToPatchPairToInitialSplitDataNotZerg;
        std::unordered_map<PositionAndVelocity, std::map<std::pair<TilePosition, TilePosition>, InitialSplitData>>
            startLocationToPatchPairToInitialSplitDataUnknown;

        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            positionDeltas.clear();
            minimumNextPathLength = 0;
            resourceToSerializedGatherPaths.clear();
            resourceToSerializedReturnPaths.clear();
            startLocationToPatchPairToInitialSplitDataZerg.clear();
            startLocationToPatchPairToInitialSplitDataNotZerg.clear();
            startLocationToPatchPairToInitialSplitDataUnknown.clear();
        }

        static double occurrenceRateToProbability(uint8_t occurrenceRate)
        {
            return (double)occurrenceRate / (double)OCCURRENCE_SCALE;
        }

        // Ensure we never copy map data
        [[no_unique_address]] noncopyable _ = {};
    };
}
