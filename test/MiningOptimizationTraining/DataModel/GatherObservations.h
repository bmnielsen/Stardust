#pragma once

#include "BWTest.h"
#include "ArrivalData.h"
#include "PositionOnPath.h"

namespace MiningOptimizationTraining
{
    struct GatherArrivalObservations
    {
        // The arrival data (delay and facing patch) with their occurrences
        std::unordered_map<ArrivalData, uint32_t> arrivalToOccurrences;

        // How many arrivals had a collision
        uint32_t collisions = 0;

        // How many arrivals did not have a collision
        uint32_t nonCollisions = 0;
    };

    // This structure stores a node in a gather path
    // The path may branch because of a resend taking effect at this position or because of subpixel instability in the path
    struct GatherObservations
    {
    public:
        // The position and related data, stored in many different ways
        PositionOnPath pos;

        // How often this position has occurred in its path
        // For root nodes, how often it has been observed
        uint32_t occurrences = 0;

        // The arrival observations from this node when the path is not changed by a later gather command
        GatherArrivalObservations noResendArrivalObservations;

        // The arrival observations when a resend is sent here and the path is not changed by a later gather command
        GatherArrivalObservations resendArrivalObservations;

        // All next positions seen from this position when the path has not been changed by a resend
        // Will be empty on the last node before arrival at the patch
        std::vector<GatherObservations> nextPositions;

        // All next positions seen from this position after a resend that changes the path takes effect at this node
        // Empty for positions where the path does not change after a resend
        std::vector<GatherObservations> nextPositionsAfterResend;
    };

    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations);
}