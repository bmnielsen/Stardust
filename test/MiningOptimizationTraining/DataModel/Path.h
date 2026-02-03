#pragma once

#include "Noncopyable.h"

#include "BWTest.h"
#include "PositionAndVelocity.h"
#include "Configuration.h"

namespace MiningOptimizationTraining
{
    enum class NodeType:uint8_t
    {
        Uninitialized,              // Initial state
        AfterExplorationWindow,     // A node that occurs after our exploration window, so we don't explore it
        StableNode,                 // A node where resends do not change the path
        PoorResendNode,             // A node early in the path that results in a longer path to the patch
        NonfinalResendNode,         // A node where resends change the path, and additional resends may be sent later
        FinalResendNode,            // A node where resends change the path, but no additional resends may be sent
        ResendUnavailable,          // A node where resends cannot occur because of Unit_Busy or because it is too early in the path
        Test,                       // Used in unit tests
    };

    // This structure stores a node in a path
    // The path may branch because of a resend taking effect at this position or because of subpixel instability in the path
    template <typename ObservationType>
    struct PathNode
    {
        // The position, including velocity and heading
        PositionAndVelocity pos;

        // The type of node, see definition of NodeType for details on each type
        NodeType type = NodeType::Uninitialized;

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
        std::vector<std::pair<PathNode<ObservationType>, uint32_t>> nextPositions;

        // The number of times this root node has been explored with no collision at the start of the path
        uint32_t timesExploredWithNoCollision = 0;

        // The number of times this root node has been explored with a collision at the start of the path
        uint32_t timesExploredWithCollision = 0;

        // Counters for our best arrival delays and their occurrences
        // Best arrival delay is whatever combination of resends gave the best result on a given exploration
        std::map<uint16_t, uint32_t> bestArrivalDelaysAndOccurrences;

        // The set of exact positions left to explore from this root position
        // These are only set on return paths and serve as the starting points for our path exploration
        std::vector<BWAPI::ExactPosition> positionsToExplore;

        void populatePositionsToExplore()
        {
            auto baseX = ((unsigned int)pos.x) << 8;
            auto baseY = ((unsigned int)pos.y) << 8;
            for (int subpixelX = 0; subpixelX < 256; subpixelX += (256 / EXACT_POSITIONS_TO_EXPLORE_PER_AXIS))
            {
                for (int subpixelY = 0; subpixelY < 256; subpixelY += (256 / EXACT_POSITIONS_TO_EXPLORE_PER_AXIS))
                {
                    positionsToExplore.emplace_back(baseX + subpixelX, baseY + subpixelY, pos.heading, 0, 0);
                    std::cout << "Added " << *positionsToExplore.rbegin() << std::endl;
                }
            }
        }

        // Ensure we never copy paths
        [[no_unique_address]] noncopyable _ = {};
    };
}
