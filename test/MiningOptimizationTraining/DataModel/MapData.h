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
        struct OrderProcessTimerReset
        {
            // The order process timer value reset to
            uint8_t value;

            // Whether the opponent is zerg
            bool opponentIsZerg;

            // The count of opponent start locations this reset value applies to
            uint8_t opponentStartLocationsCount;

            // A random seed that gives this order process timer reset
            uint32_t randomSeed;

            friend std::ostream &operator << (std::ostream &os, const OrderProcessTimerReset &orderProcessTimerReset)
            {
                os << "[" << (unsigned int)orderProcessTimerReset.value;
                os << ";" << (orderProcessTimerReset.opponentIsZerg ? "zerg" : "notzerg");
                os << ";" << (unsigned int)orderProcessTimerReset.opponentStartLocationsCount;
                os << "]";
                return os;
            };
        };

        std::string mapHash;

        // This maps out the possible order process timer reset values each starting worker can get
        // Because the values depend on each unit's position in the visible unit list, they vary based on start location and opponent race
        // For race, the variation is between zerg and non-zerg, as zerg start with two extra units and the larva is always initialized last
        // For start location, the variation occurs because our units end up ahead or behind the opponent's based on whether our start location is
        // ahead or behind theirs
        std::map<BWAPI::Position, std::vector<OrderProcessTimerReset>> startingWorkerPositionToOrderProcessTimerReset;

        // The game randomizes the heading of each starting worker, but aligned to intervals of 8 giving 32 possible values.
        // For each starting position (including heading), we simulate the first two full rotations with all combinations of patches and all
        // possible order process timer reset effects.
        // When the bot starts up, this allows it to find the allocation of starting workers that gives the earliest 7th collection.
        std::map<BWAPI::ExactPosition, std::map<TilePosition, InitialWorkerGatherPathNode>> startingWorkerPositionToPatchToGatherPaths;
        std::map<BWAPI::ExactPosition, std::map<TilePosition, InitialWorkerReturnPathNode>> startingWorkerPositionToPatchToReturnPaths;

        // The number of starting positions (with heading) that are left to explore
        std::vector<BWAPI::ExactPosition> positionsToExplore;

        void clear(const std::string &_mapHash)
        {
            mapHash = _mapHash;
            startingWorkerPositionToOrderProcessTimerReset.clear();
        }

        // Ensure we never copy map data
        [[no_unique_address]] noncopyable _ = {};
    };
}
