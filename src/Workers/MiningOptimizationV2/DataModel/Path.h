#pragma once

#include "Common.h"
#include "Noncopyable.h"

#include "PositionAndVelocity.h"
#include "PositionDeltaAndVelocity.h"

namespace MiningOptimization
{
    // This structure stores a node in a path
    // The path may branch because of a resend taking effect at this position or because of subpixel instability in the path
    template <typename ObservationType>
    struct PathNode
    {
        // The position delta, which includes velocity and heading when needed
        PositionDeltaAndVelocity pos;

        // The arrival observations on this node if it is the final node in the path, i.e. the node immediately before arrival
        // Contains default data on all nodes where nextPositions is non-empty
        std::vector<std::pair<ObservationType, uint8_t>> arrivalDataWhenFinalNode;

        // The arrival observations when a final resend takes effect here, meaning we don't track the rest of the path
        // Sorted from highest occurrence rate to lowest
        std::vector<std::pair<ObservationType, uint8_t>> arrivalDataAfterResend;

        // All next positions seen from this position when the path has not been changed by a resend
        // Will be empty on the last node before arrival at the patch
        // Sorted from highest occurrence rate to lowest
        std::vector<std::pair<PathNode<ObservationType>, uint8_t>> nextPositions;

        // All next positions seen from this position after a resend takes effect at this node
        // Will be empty on any nodes where resends do not change the path or no additional resends are possible
        // Sorted from highest occurrence rate to lowest
        std::vector<std::pair<PathNode<ObservationType>, uint8_t>> nextPositionsAfterResend;

        // Whether this is a stable resend node, where a resend taking effect does not change the path
        // Both stable nodes and nodes outside the exploration window are stored without data in the resend vectors, so this boolean is needed to
        // differentiate the two cases.
        bool isStableResendNode = false;

        // Returns a reference to the appropriate next positions vector depending on whether a resend is taking effect here or not.
        const std::vector<std::pair<PathNode<ObservationType>, uint8_t>> &applicableNextPositions(int frame,
                                                                                                  const auto &previousResendFrames) const
        {
            if (previousResendFrames.contains(frame - BWAPI::Broodwar->getLatencyFrames()))
            {
                return nextPositionsAfterResend;
            }
            return nextPositions;
        }

        // Ensure we never copy path nodes
        [[no_unique_address]] noncopyable _ = {};
    };

    // Stores the root of a path
    template <typename ObservationType>
    struct Path
    {
        // The position, including velocity and heading
        PositionAndVelocity pos;

        // All next positions seen from this node
        std::vector<std::pair<PathNode<ObservationType>, uint8_t>> nextPositions;

        // Ensure we never copy paths
        [[no_unique_address]] noncopyable _ = {};
    };
}
