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

    class InitialWorkerMapData
    {
    public:
        enum class OpponentRace:uint8_t
        {
            Meaningless,
            Zerg,
            NotZerg
        };

        struct OrderProcessTimerReset
        {
            // The order process timer value reset to
            uint8_t value;

            // What opponent race configuration this reset applies to
            OpponentRace opponentRace;

            // The count of opponent start locations this reset value applies to
            uint8_t opponentStartLocationsCount;

            // A random seed that gives this order process timer reset
            uint32_t randomSeed;
        };

        std::string mapHash;

        // This maps out the possible order process timer reset values each starting worker can get
        // Because the values depend on each unit's position in the visible unit list, they vary based on start location and opponent race
        // If our units are first in the list, the value is always the same, so one start position always gives completely known values
        // If our units are last in the list, the value depends on whether the opponent is zerg or not, since zerg gets two extra units at game start
        std::map<BWAPI::Position, std::vector<OrderProcessTimerReset>> startingWorkerPositionToOrderProcessTimerReset;

        // The game randomizes the heading of each starting worker, but aligned to intervals of 8 so there are 32 possible values.
        // For each starting position (including heading), we simulate the first two full rotations with all combinations of patches and all
        // possible order process timer reset values.
        // When the bot starts up, this allows it to find the allocation of starting workers that gives the earliest 7th collection.


        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            startingWorkerPositionToOrderProcessTimerReset.clear();
        }

        // Ensure we never copy map data
        [[no_unique_address]] noncopyable _ = {};
    };
}
