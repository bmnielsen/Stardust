#pragma once

#include "Path.h"

#include <cstdint>

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
     * We store the next path length as an increment from the minimum path length in MapData. This allows it to fit into 6 bits, which allows us
     * to pack the collision and facing target data alongside it in a single 8-bit integer.
     */
    struct GatherArrivalData
    {
        uint8_t arrivalDelay = UINT8_MAX;
        uint8_t packed = UINT8_MAX;

        // The length of the return path from the end position of this arrival
        [[nodiscard]] unsigned int nextPathLength(uint8_t minimumNextPathLength) const
        {
            // Next path length is stored in the upper 6 bits, so shift two right and add the minimum value
            return (unsigned int)(packed >> 2) + minimumNextPathLength;
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
            return std::tie(arrivalDelay, packed) == std::tie(other.arrivalDelay, other.packed);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(arrivalDelay, packed) < std::tie(other.arrivalDelay, other.packed);
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(arrivalDelay);
            s.value1b(packed);
        }

        friend std::ostream& operator<< (std::ostream& os, const GatherArrivalData& data)
        {
            os << data.arrivalDelay;
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
