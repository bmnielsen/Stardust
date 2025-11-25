#pragma once

#include "../DataModel/Path.h"
#include "Resource.h"

namespace MiningOptimization
{
    namespace {
        std::array<double, GATHER_FORECAST_FRAMES> emptyOtherPatchesForecast;
    }

    template <typename ObservationType>
    struct SolverResult
    {
        // The frame on which to reconsider the path
        int reconsiderationFrame;

        // The frames on which to resend the relevant command
        std::set<int> resendFrames;

        // The most likely path that will be followed, from the first next node to the final resend node or arrival node
        std::deque<PathNode<ObservationType>*> expectedPath;

        // The different arrival frames we expect to encounter from the planned resends

    };

    template <typename ObservationType>
    class Solver
    {
    public:
        // Constructor used for single-worker gathering and return
        Solver(const PositionAndVelocity &startPosition,
               const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextPathNodes,
               const std::set<int> &previousResendFrames,
               int startFrame,
               int workerOrderProcessTimerAtStartFrame)
                : startPosition(startPosition)
                , initialNextPathNodes(nextPathNodes)
                , initialPreviousResendFrames(previousResendFrames)
                , startFrame(startFrame)
                , workerOrderProcessTimerAtStartFrame(workerOrderProcessTimerAtStartFrame)
                , takeoverFrame(-1)
                , otherPatchesForecast(emptyOtherPatchesForecast)
        {}

        // Constructor used for double-worker gathering
        Solver(const PositionAndVelocity &startPosition,
               const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextPathNodes,
               const std::set<int> &previousResendFrames,
               int startFrame,
               int workerOrderProcessTimerAtStartFrame,
               int takeoverFrame,
               const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast)
                : startPosition(startPosition)
                , initialNextPathNodes(nextPathNodes)
                , initialPreviousResendFrames(previousResendFrames)
                , startFrame(startFrame)
                , workerOrderProcessTimerAtStartFrame(workerOrderProcessTimerAtStartFrame)
                , takeoverFrame(takeoverFrame)
                , otherPatchesForecast(otherPatchesForecast)
        {}

        void execute();

    private:
        // The start position of this solver execution
        const PositionAndVelocity &startPosition;

        // The next path nodes and their weights from the start position
        const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &initialNextPathNodes;

        // Any previous resend frames prior to this solver execution
        // Relevant when we are re-running the solver because the worker is following a different path than expected
        const std::set<int> &initialPreviousResendFrames;

        // The start frame of this solver execution
        int startFrame;

        // The worker's order process timer value at the start frame, or -1 if this is unknown
        int workerOrderProcessTimerAtStartFrame;

        // The frame when this worker is guaranteed to be able to take over from another worker, or -1 if this is a single-worker or return case
        int takeoverFrame;

        // The forecast of whether all other patches will be gathered in the frames after the start frame
        // Only relevant for two-worker gather, otherwise it will be set to a default value
        const std::array<double, GATHER_FORECAST_FRAMES> &otherPatchesForecast;

        struct ArrivalDetails
        {
            ObservationType arrivalData;
            int workerOrderProcessTimerAtArrival;
        };
        typedef std::vector<std::pair<ArrivalDetails, double>> NoResendArrivalDetails;

        NoResendArrivalDetails processNextNodes(const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextPathNodes,
                                                int frame,
                                                std::set<int> &previousResendFrames,
                                                int workerOrderProcessTimer);
    };
}
