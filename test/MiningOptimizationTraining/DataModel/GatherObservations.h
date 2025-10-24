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

        void addArrivalObservation(ArrivalData arrivalData)
        {
            auto it = arrivalToOccurrences.find(arrivalData);
            if (it == arrivalToOccurrences.end())
            {
                arrivalToOccurrences[arrivalData] = 1;
                return;
            }
            if (it->second < UINT32_MAX) it->second++;
        }

        void addCollisionObservation(bool collision)
        {
            if ((collisions + nonCollisions) == UINT32_MAX) return;
            (collision ? collisions : nonCollisions)++;
        }
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
        GatherArrivalObservations arrivalObservations;

        // The arrival observations when a resend takes effect here
        GatherArrivalObservations arrivalObservationsAfterResend;

        // All next positions seen from this position when the path has not been changed by a resend
        // Will be empty on the last node before arrival at the patch
        std::vector<GatherObservations> nextPositions;

        // All next positions seen from this position after a resend takes effect at this node
        // For training, we do not differentiate between resends that change the path and resends that do not
        std::vector<GatherObservations> nextPositionsAfterResend;
    };

    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations);
}