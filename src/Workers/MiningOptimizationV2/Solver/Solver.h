#pragma once

#include "Resource.h"
#include "../DataModel/MapData.h"
#include "../DataModel/Path.h"

#include "SolverResult.h"

namespace MiningOptimization
{
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
        Solver(const MapData &mapData,
               Resource resource,
               const PositionAndVelocity &startPosition,
               const Path<ObservationType> &path,
               int startFrame,
               std::multiset<int> _possibleWorkerOrderProcessTimerValuesAtStartFrame)
                : mapData(mapData)
                , resource(std::move(resource))
                , startPosition(startPosition)
                , path(path)
                , startFrame(startFrame)
                , possibleWorkerOrderProcessTimerValuesAtStartFrame(std::move(_possibleWorkerOrderProcessTimerValuesAtStartFrame))
        {
            if (possibleWorkerOrderProcessTimerValuesAtStartFrame.empty())
            {
                possibleWorkerOrderProcessTimerValuesAtStartFrame = {0, 1, 2, 3, 4, 5, 6, 7, 8};
            }
        }

        // Executes the solver
        SolverResult<ObservationType> execute();

    private:
        // We keep a reference to the map data so we can interpret some of the data correctly
        const MapData &mapData;

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

        // Recursively process the given next nodes, returning the best solution for all paths below them
        SolverResult<ObservationType> processNextNodes(
                const PositionAndVelocity &pos,
                const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &nextPathNodes,
                int frame,
                int tenDistanceFrame,
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
