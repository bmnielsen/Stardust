#pragma once

#include "../MiningOptimizationConfiguration.h"
#include "Path.h"

#include <cstdint>

namespace MiningOptimization
{
    /*
     * Stores the arrival data we need to track for gather paths.
     *
     * Arrival delay: the number of frames to arrival at the target
     * Facing target: whether the worker is facing its target at arrival
     * Collision: whether there was a collision when the worker leaves the target again
     * Ten distance delta: The number of frames from the arrival frame where the worker is 10 distance from the patch
     * Resend always arrives delta: The number of frames from the arrival frame where a resend will always reach the patch on time
     * Next path length (currently disabled): average length of the return path from the end position of this arrival
     *
     * We store the next path length as an increment from the minimum path length in MapData. This allows it to fit into 6 bits, which allows us
     * to pack the collision and facing target data alongside it in a single 8-bit integer.
     */
    struct GatherArrivalData
    {
        uint8_t packed = UINT8_MAX;
        uint8_t tenDistanceAndResendAlwaysArrivesIndex = 0;

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

#if USE_NEXT_PATH_LENGTHS
        uint8_t nextPathLengthDelta = UINT8_MAX;

        // The length of the return path from the end position of this arrival
        [[nodiscard]] unsigned int nextPathLength(uint8_t minimumNextPathLength) const
        {
            // Next path length is stored in the upper 6 bits, so shift two right and add the minimum value
            return (unsigned int)nextPathLengthDelta + minimumNextPathLength;
        }

        bool operator==(const GatherArrivalData &other) const
        {
            return std::tie(packed, nextPathLengthDelta) == std::tie(other.packed, other.nextPathLengthDelta);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(packed, nextPathLengthDelta) < std::tie(other.packed, other.nextPathLengthDelta);
        }
#else
        bool operator==(const GatherArrivalData &other) const
        {
            return std::tie(packed, tenDistanceAndResendAlwaysArrivesIndex) == std::tie(other.packed, other.tenDistanceAndResendAlwaysArrivesIndex);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(packed, tenDistanceAndResendAlwaysArrivesIndex) < std::tie(other.packed, other.tenDistanceAndResendAlwaysArrivesIndex);
        }
#endif

        // Adds the delay after mining start to the given map
        // Possible delays for gather:
        // - Not facing patch when transitioning to mine incurs an order process timer cycle of delay for the worker to turn
        // - A collision after mining completion incurs an order process timer cycle of delay for the worker to resolve the collision
        // - An order process timer reset during the transition to mining incurs a delay before the mining timer starts counting down
        // The two first points can be shortened or lengthened if there is an order process timer reset during the delay, but this is such an
        // extreme edge case that we don't consider it.
        void addDelayAfterAction(std::map<int, double> &delaysWithProbabilities,
                                 int orderProcessTimerAtArrival,
                                 int actionFrame,
                                 double baseProbability) const;

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
