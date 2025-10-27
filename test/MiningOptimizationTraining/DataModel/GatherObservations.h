#pragma once

#include "BWTest.h"
#include "ArrivalData.h"
#include "PositionOnPath.h"

// Defines the exploration horizon in number of frames to arrival
#define EXPLORATION_WINDOW_START 20
#define EXPLORATION_WINDOW_END 5

namespace MiningOptimizationTraining
{
    enum class ResendChangesPath:uint8_t
    {
        Unknown,    // Have not explored this yet
        Yes,        // A resend could change the path (there are false positives)
        No          // A resend will not change the path
    };

    struct GatherArrivalObservations
    {
        // The arrival data (delay and facing patch) with their occurrences
        std::map<ArrivalData, uint32_t> arrivalToOccurrences;

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

        ArrivalData mostCommonArrivalData() const;
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

        // Whether a resend taking effect at this position could change the path.
        // This is determined by running an openbw pathfind and checking if the next waypoint differs from the current one. This does generate
        // false positives but is still useful for reducing the optimization search space
        ResendChangesPath canResendChangePath = ResendChangesPath::Unknown;

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

        // Whether this position is within the exploration window (the set of frames-before-arrival that we explore within)
        bool withinExplorationWindow() const;
    };

    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations);
}