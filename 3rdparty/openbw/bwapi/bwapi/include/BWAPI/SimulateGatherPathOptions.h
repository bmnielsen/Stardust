#pragma once

#include <set>

namespace BWAPI
{
    namespace
    {
        std::set<int> emptyResendFrames;
    }

    struct SimulateGatherPathOptions
    {
        SimulateGatherPathOptions() : resendFrames(emptyResendFrames) {};
        explicit SimulateGatherPathOptions(const std::set<int> &resendFrames) : resendFrames(resendFrames) {};

        // Forces the action (start of gather or delivery of resources) to occur either at arrival or after arrival depending on the given bool
        // If this is not called, the action will occur at the natural timing based on the unit's order process timer
        SimulateGatherPathOptions &setForceAction(bool atArrival)
        {
            forceActionAtArrival = atArrival;
            forceActionAfterArrival = !atArrival;
            return *this;
        }

        const std::set<int> &resendFrames;
        bool forceActionAtArrival = false;
        bool forceActionAfterArrival = false;
    };
}
