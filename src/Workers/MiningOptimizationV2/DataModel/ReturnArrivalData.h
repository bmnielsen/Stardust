#pragma once

#include "../MiningOptimizationConfiguration.h"
#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>
#include <algorithm>

namespace MiningOptimization
{
    enum class ReturnExitSpeed:uint8_t
    {
        Collision,  // The worker collided with the depot when trying to leave it
        Low,        // The worker stopped at the depot and will therefore accelerate slowly towards the patch
        Medium,     // The worker maintained some speed after delivery
        High,       // The worker maintained a great deal of speed after delivery
    };

    std::ostream& operator<<(std::ostream& os, const ReturnExitSpeed &exitSpeed);

    /*
     * Stores the arrival data we need to track for return paths.
     *
     * Arrival delay: the number of frames to arrival at the depot
     * Collision: whether the worker collides with the depot after delivery when delivery does not happen on the first frame
     * Exit speed: the exit speed from the depot when delivery happens on the first frame
     * Next path length (currently disabled): average length of the gather path from the end position of this arrival
     *
     * We store the next path length as an increment from the minimum path length in MapData. This allows it to fit into 6 bits, which allows us
     * to pack the exit speed data alongside it in a single 8-bit integer.
     */
    struct ReturnArrivalData
    {
        uint8_t packed = UINT8_MAX;
        bool collision = false;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 6 bits, so shift two right and return
            return packed >> 2;
        }

        // The exit speed of the worker from the depot back towards the patch when there was delivery at arrival
        [[nodiscard]] ReturnExitSpeed exitSpeed() const
        {
            // Exit speed is stored in the lowest two bits
            return (ReturnExitSpeed)(packed & 0b00000011);
        }

#if USE_NEXT_PATH_LENGTHS
        uint8_t nextPathLengthDelta = UINT8_MAX;

        // The length of the gather path from the end position of this arrival
        [[nodiscard]] unsigned int nextPathLength(uint8_t minimumNextPathLength) const
        {
            // Next path length is stored in the upper 6 bits, so shift two right and add the minimum value
            return (unsigned int)nextPathLengthDelta + minimumNextPathLength;
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathLengthDelta) == std::tie(other.packed, other.nextPathLengthDelta);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathLengthDelta) < std::tie(other.packed, other.nextPathLengthDelta);
        }
#else
        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packed, collision) == std::tie(other.packed, other.collision);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed, collision) < std::tie(other.packed, other.collision);
        }
#endif

        // Adds the delay after the return to the given map
        // If the order process timer at arrival is 0, the delay is allowed to be negative if speed is kept
        // Otherwise only collisions are considered
        void addDelayAfterAction(std::map<int, double> &delaysWithProbabilities,
                                 int orderProcessTimerAtArrival,
                                 int actionFrame,
                                 double baseProbability) const;

        friend std::ostream& operator<< (std::ostream& os, const ReturnArrivalData& data)
        {
            os << data.arrivalDelay();
            switch (data.exitSpeed())
            {
                case ReturnExitSpeed::Collision:
                    os << "[c]";
                    break;
                case ReturnExitSpeed::Low:
                    os << "[l]";
                    break;
                case ReturnExitSpeed::Medium:
                    os << "[m]";
                    break;
                case ReturnExitSpeed::High:
                    os << "[h]";
                    break;
            }
            if (data.collision)
            {
                os << "{c}";
            }
            return os;
        }
    };

    typedef Path<ReturnArrivalData> ReturnPath;
    typedef PathNode<ReturnArrivalData> ReturnPathNode;
}
