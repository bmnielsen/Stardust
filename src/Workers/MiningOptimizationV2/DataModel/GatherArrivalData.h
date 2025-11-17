#pragma once

#include "Path.h"

#include <cstdint>

#define UINT6_MAX 63U

namespace MiningOptimizationV2
{
    /*
     * Stores the arrival data we need to track for gather paths.
     *
     * Arrival delay: the number of frames to arrival at the target
     * Facing target: whether the worker is facing its target at arrival
     * Collision: whether there was a collision when the worker leaves the target again
     * Next path length: average length of the return path from the end position of this arrival
     *
     * We patch the collision and facing target data into the arrival delay, since we don't need the full 8 bits.
     */
    struct GatherArrivalData
    {
        uint8_t packed = UINT8_MAX;
        uint8_t nextPathLength = UINT8_MAX;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 6 bits, so shift two right and return
            return packed >> 2;
        }

        // Whether the worker is facing its target at arrival
        [[nodiscard]] bool facingTarget() const
        {
            // Lowest bit is set if the worker is not facing the target
            return (packed & 0b00000001) == 0;
        }

        // Whether there was a collision
        [[nodiscard]] bool collision() const
        {
            // Second-lowest bit is set if the worker collided
            return (packed & 0b00000010) == 0b00000010;
        }

        bool operator==(const GatherArrivalData &other) const
        {
            return std::tie(packed, nextPathLength) == std::tie(other.packed, other.nextPathLength);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(packed, nextPathLength) < std::tie(other.packed, other.nextPathLength);
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(packed);
            s.value1b(nextPathLength);
        }

        friend std::ostream& operator<< (std::ostream& os, const GatherArrivalData& data)
        {
            os << data.arrivalDelay();
            if (!data.facingTarget())
            {
                os << "[!f]";
            }
            if (!data.collision())
            {
                os << "[c]";
            }
            return os;
        }
    };

    typedef Path<GatherArrivalData> GatherPath;
    typedef PathNode<GatherArrivalData> GatherPathNode;
}
