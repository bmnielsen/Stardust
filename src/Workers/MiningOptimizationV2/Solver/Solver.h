#pragma once

#include "Resource.h"
#include "../DataModel/Path.h"

#include "SolverResult.h"

#define PATCH_LOCK_THRESHOLD 0.8

namespace MiningOptimization
{
    namespace {
        ResourceGatherProbabilityForecast emptyOtherPatchesForecast;
    }

    // Wrapper for the resend frames allowing us to also store some metadata
    struct SolverResends
    {
        // The frames on which to resend
        std::set<int> resendFrames;

        // Whether the final resend has been planned
        bool isFinal = false;
    };

    template <typename ObservationType>
    class Solver
    {
    public:
        // Constructor used for single-worker gathering and return
        Solver(const std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
               const unsigned int minimumNextPathLength,
               Resource resource,
               const PositionAndVelocity &startPosition,
               const Path<ObservationType> &path,
               int startFrame,
               std::multiset<int> _possibleWorkerOrderProcessTimerValuesAtStartFrame)
                : positionDeltas(positionDeltas)
                , minimumNextPathLength(minimumNextPathLength)
                , resource(std::move(resource))
                , startPosition(startPosition)
                , path(path)
                , startFrame(startFrame)
                , possibleWorkerOrderProcessTimerValuesAtStartFrame(std::move(_possibleWorkerOrderProcessTimerValuesAtStartFrame))
                , takeoverFrame(-1)
                , otherPatchesForecast(emptyOtherPatchesForecast)
        {
            if (possibleWorkerOrderProcessTimerValuesAtStartFrame.empty())
            {
                possibleWorkerOrderProcessTimerValuesAtStartFrame = {0, 1, 2, 3, 4, 5, 6, 7, 8};
            }
        }

        // Constructor used for double-worker gathering
        Solver(const std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
               const unsigned int minimumNextPathLength,
               Resource resource,
               const PositionAndVelocity &startPosition,
               const Path<ObservationType> &path,
               int startFrame,
               std::multiset<int> _possibleWorkerOrderProcessTimerValuesAtStartFrame,
               int takeoverFrame,
               const ResourceGatherProbabilityForecast &otherPatchesForecast)
                : positionDeltas(positionDeltas)
                , minimumNextPathLength(minimumNextPathLength)
                , resource(std::move(resource))
                , startPosition(startPosition)
                , path(path)
                , startFrame(startFrame)
                , possibleWorkerOrderProcessTimerValuesAtStartFrame(std::move(_possibleWorkerOrderProcessTimerValuesAtStartFrame))
                , takeoverFrame(takeoverFrame)
                , otherPatchesForecast(otherPatchesForecast)
        {
            if (possibleWorkerOrderProcessTimerValuesAtStartFrame.empty())
            {
                possibleWorkerOrderProcessTimerValuesAtStartFrame = {0, 1, 2, 3, 4, 5, 6, 7, 8};
            }
        }

        // Executes the solver
        SolverResult<ObservationType> execute();

    private:
        /* References to the map mining optimization data relevant for this solve */

        const std::vector<std::pair<int8_t, int8_t>> &positionDeltas;
        const unsigned int minimumNextPathLength;

        Resource resource;

        // The start position of this solver execution
        const PositionAndVelocity &startPosition;

        // The path
        const Path<ObservationType> &path;

        // The start frame of this solver execution
        int startFrame;

        // The worker's possible order process timer value at the end of the start frame
        // May be empty if the order process timer values are completely unknown
        std::multiset<int> possibleWorkerOrderProcessTimerValuesAtStartFrame;

        // The frame when this worker is guaranteed to be able to take over from another worker, or -1 if this is a single-worker or return case
        int takeoverFrame;

        // The forecast of whether all other patches will be gathered in the frames after the start frame
        // Only relevant for two-worker gather, otherwise it will be set to a default value
        const ResourceGatherProbabilityForecast &otherPatchesForecast;

        // Recursively process the given next nodes, returning the best solution for all paths below them
        SolverResult<ObservationType> processNextNodes(
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextPathNodes,
                int frame,
                const SolverResends &previousResends,
                const std::multiset<int> &workerOrderProcessTimer) const;

        // Whether a resend is viable from the given node on the given frame with the given previous resend frames
        // A resend is viable if it can be issued and all possible resend nodes are either stable or have resend data available
        // The second return value is whether the resend must be the final resend, and is only relevant if the first value is true
        std::pair<bool, bool> isResendViableHere(const PathNode<ObservationType> &node,
                                                 int frame,
                                                 const SolverResends &previousResends) const;

        /*
         * These methods are where the logic differs between gather and return paths. They are implemented in their own files for each
         * template specialization.
         */

        // Checks if a resend can be sent on the given frame
        [[nodiscard]] bool canResendOnFrame(int frame, const std::set<int> &previousResendFrames) const;

        // How many frames it takes to transition to the action
        int transitionFramesToAction() const;
    };
}
