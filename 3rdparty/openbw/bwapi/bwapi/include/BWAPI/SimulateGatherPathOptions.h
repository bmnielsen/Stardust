#pragma once

#include <set>

// Forwards
namespace BWAPI
{
    struct SimulateGatherPathResult;
}
namespace bwgame
{
    struct state;
}

namespace BWAPI
{
    struct SimulateGatherPathOptions
    {
        SimulateGatherPathOptions();
        explicit SimulateGatherPathOptions(const std::set<int> &resendFrames);
        explicit SimulateGatherPathOptions(const std::unique_ptr<bwgame::state> &startingState);
        explicit SimulateGatherPathOptions(const std::set<int> &resendFrames, const std::unique_ptr<bwgame::state> &startingState);
        ~SimulateGatherPathOptions();

        // Forces the action (start of gather or delivery of resources) to occur either at arrival or after arrival depending on the given bool
        // If this is not called, the action will occur at the natural timing based on the unit's order process timer
        SimulateGatherPathOptions &setForceAction(bool atArrival);

        // Tells the simulator that we want a copy of the state returned at the start of the next path, which we can use in later invocations to
        // simulate the start of the next 'path'
        SimulateGatherPathOptions &setReturnStateAtStartOfNextPath();

        // Sets that we want to switch to the given patch at the start of the simulation
        SimulateGatherPathOptions &switchToPatch(size_t _patchUnitIndex);

        // Indicate that the result should include all positions visited by the worker, not just positions after the last resend
        SimulateGatherPathOptions &setIncludeAllPositions();

        // Have the simulation skip the first frame
        // Chaining simulated results together ends up skipping the first frame of each step, as it is the end frame of the last one, so this lets
        // us align the first result to this pattern
        SimulateGatherPathOptions &setSkipFirstFrame();

        const std::set<int> &resendFrames;
        const std::unique_ptr<bwgame::state> &startingState;
        bool forceActionAtArrival = false;
        bool forceActionAfterArrival = false;
        bool returnStateAtStartOfNextPath = false;
        bool switchPatches = false;
        size_t patchUnitIndex = 0;
        bool includeAllPositions = false;
        bool skipFirstFrame = false;
    };
}
