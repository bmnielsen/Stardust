#pragma once

#include "BWAPI.h"
#include "MiningOptimizationTraining/DataModel/MapData.h"

namespace MiningOptimizationTraining
{
    // Struct to store the results of parsing the position history
    struct ParsedPositionHistory
    {
        // Whether the path is valid
        bool valid;

        // Whether the worker is pointed at its target at the end of the path
        bool facingTarget;

        // Iterator in the position history to the arrival position
        std::vector<std::shared_ptr<const PositionOnPath>>::iterator arrivalPositionIt;

        // Iterators in the position history to the positions where command resends took effect
        std::set<std::vector<std::shared_ptr<const PositionOnPath>>::iterator> resendPositionIts;

        explicit ParsedPositionHistory(std::vector<std::shared_ptr<const PositionOnPath>> &positionHistory)
            : valid(false)
            , facingTarget(true)
            , arrivalPositionIt(positionHistory.end())
        {}
    };

    class WorkerPathExploration
    {
    public:
        WorkerPathExploration(MapData &mapData, BWAPI::Unit worker, BWAPI::Unit patch, BWAPI::Unit depot)
            : mapData(mapData)
            , worker(worker)
            , patch(patch)
            , depot(depot)
            , previousPosition(SubpixelPosition(worker))
            , state(0)
        {}

        void update();

    private:
        MapData &mapData;

        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // Stores the previous subpixel position of the worker
        SubpixelPosition previousPosition;

        // History of positions visited while on the way to gather
        std::vector<std::shared_ptr<const PositionOnPath>> gatherPositionHistory;

        // Pointer to the gather node at the current position
        // Will be null if there is no saved data yet for this position
        GatherObservations* currentGatherNode = nullptr;

        // History of positions visited while returning
        std::vector<std::shared_ptr<const PositionOnPath>> returnPositionHistory;

        // State for the state machine. Possible values:
        // 0 - approaching the patch
        // 1 - mining
        // 2 - approaching the depot
        int state;

        // Called for workers that are on their way to gather minerals
        void gathering();

        // Called when the worker is transitioning to mining to record the path
        void recordGatherPath();

        // Called when the worker has started returning minerals and we know if it collided with the patch or not
        void recordGatherCollisions();

        // Called for workers that are on their way to return minerals
        void returning();

        void appendCurrentPosition(std::vector<std::shared_ptr<const PositionOnPath>> &vector)
        {
            // The first node ignores the subpixel change
            vector.emplace_back(vector.empty()
                                ? std::make_shared<PositionOnPath>(worker)
                                : std::make_shared<PositionOnPath>(worker, previousPosition));
        }

        // Parses data like the arrival and resend positions out of the position history
        // Start and target are the start and end units visited by the worker on the path, so e.g. for a gather path, start will be the depot and
        // target will be the patch.
        // If the positions are wrong for whatever reason, valid will be set to false.
        ParsedPositionHistory parsePositionHistory(
                std::vector<std::shared_ptr<const PositionOnPath>> &positionHistory,
                BWAPI::Unit start,
                BWAPI::Unit target);
    };
}
