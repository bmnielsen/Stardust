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
            , previousPosition(worker->getExactPosition())
            , gatherPositionHistoryStartFrame(-1)
            , currentGatherNode(nullptr)
            , returnPositionHistoryStartFrame(-1)
            , resendsPlanned(false)
            , state(-1)
        {}

        void update();

    private:
        MapData &mapData;

        BWAPI::Unit worker;
        BWAPI::Unit patch;
        BWAPI::Unit depot;

        // Stores the previous exact position of the worker (including subpixels)
        BWAPI::ExactPosition previousPosition;

        /* State specific to the gather phase */

        // History of positions visited while on the way to gather
        std::vector<std::shared_ptr<const PositionOnPath>> gatherPositionHistory;

        // Temporarily added for validation
        std::vector<BWAPI::ExactPosition> gatherExactPositionHistory;
        std::vector<BWAPI::ExactPosition> expectedExactPositionPath;
        std::optional<bool> expectedCollision;

        // The first frame in the gather position history
        int gatherPositionHistoryStartFrame;

        // Pointer to the gather node at the current position
        // Will be null if there is no saved data yet for this position
        GatherObservations* currentGatherNode;

        // Frames with a planned gather resend
        std::set<int> plannedGatherResendFrames;

        // Frames with an executed gather resend
        std::set<int> executedGatherResendFrames;

        /* State specific to the return phase */

        // History of positions visited while returning
        std::vector<std::shared_ptr<const PositionOnPath>> returnPositionHistory;

        // The first frame in the return position history
        int returnPositionHistoryStartFrame;

        /* State used by both phases */

        // Whether any resends have been planned yet
        // Note that we can also plan to observe the path without resends, so this can be true without having any resends scheduled
        bool resendsPlanned;

        // State for the state machine. Possible values:
        // -1 - uninitialized
        // 0 - approaching the patch
        // 1 - mining
        // 2 - approaching the depot
        int state;

        // Called for workers that are on their way to gather minerals
        void gathering();

        // Plans resends for the path being followed
        void planResends();

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
                int positionHistoryStartFrame,
                std::set<int> &executedResendFrames,
                BWAPI::Unit start,
                BWAPI::Unit target);
    };
}
