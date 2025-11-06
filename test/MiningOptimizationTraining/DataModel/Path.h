#pragma once

#include "BWTest.h"
#include "PositionAndVelocity.h"

namespace MiningOptimizationTraining
{
    enum class NodeType:uint8_t
    {
        Uninitialized,              // Initial state
        BeforeExplorationWindow,    // A node that occurs before our exploration window, so we don't explore it
        AfterExplorationWindow,     // A node that occurs after our exploration window, so we don't explore it
        NonfinalResendNode,         // A node where resends change the path, and additional resends may be sent later
        FinalResendNode,            // A node where resends change the path, but no additional resends may be sent
        StableNode                  // A node within the exploration window where resends do not change the path or are impossible to issue
    };

    // This structure stores a node in a path
    // The path may branch because of a resend taking effect at this position or because of subpixel instability in the path
    template <typename ObservationType>
    struct PathNode
    {
        // The difference in x, y position between this node and the previous one, at subpixel precision
        BWAPI::ExactPositionDifference positionDifferenceFromPreviousNode;

        // The type of node, see definition of NodeType for details on each type
        NodeType type;

        // The number of times this node has been explored
        // The meaning of this depends on the node type
        // For Uninitialized, BeforeExplorationWindow and StableNode, it is unused.
        // For NonfinalResendNode, it is the number of times a resend took effect here without a subsequent resend.
        // For FinalResendNode, it is the number of times a resend took effect here.
        // For AfterExplorationWindow, it is the number of times a no-resend path reached the node.
        uint32_t timesExplored;

        // The arrival observations from this node when the path is not changed by a later gather command
        std::map<ObservationType, uint32_t> arrivalData;

        // The arrival observations when a resend takes effect here
        std::map<ObservationType, uint32_t> arrivalDataAfterResend;

        // All next positions seen from this position when the path has not been changed by a resend
        // Will be empty on the last node before arrival at the patch
        std::vector<std::pair<PathNode<ObservationType>, uint32_t>> nextPositions;

        // All next positions seen from this position after a resend takes effect at this node
        // Will be empty on any nodes where resends do not change the path or no additional resends are possible
        std::vector<std::pair<PathNode<ObservationType>, uint32_t>> nextPositionsAfterResend;
    };

    // Stores the root of a path
    template <typename ObservationType>
    struct Path
    {
        // The position, including velocity and heading
        PositionAndVelocity pos;

        // All next positions seen from this node
        std::vector<std::pair<PathNode<ObservationType>, uint32_t>> nextPositions;
    };
}
