#pragma once

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
     * Exit speed: the exit speed from the depot
     * Next path length: average length of the gather path from the end position of this arrival
     *
     * We store the next path length as an increment from the minimum path length in MapData. This allows it to fit into 6 bits, which allows us
     * to pack the exit speed data alongside it in a single 8-bit integer.
     */
    struct ReturnArrivalData
    {
        uint8_t arrivalDelay = UINT8_MAX;
        uint8_t packed = UINT8_MAX;

        // The length of the gather path from the end position of this arrival
        [[nodiscard]] unsigned int nextPathLength(uint8_t minimumNextPathLength) const
        {
            // Next path length is stored in the upper 6 bits, so shift two right and add the minimum value
            return (unsigned int)(packed >> 2) + minimumNextPathLength;
        }

        // The exit speed of the worker from the depot back towards the patch
        [[nodiscard]] ReturnExitSpeed exitSpeed() const
        {
            // Exit speed is stored in the lowest two bits
            return (ReturnExitSpeed)(packed & 0b00000011);
        }

        // Gets the delay at this arrival after the resources are delivered:
        // - Collision incurs 9 frames of delay
        // - Medium exit speed gives 2 frames of bonus
        // - High exit speed gives 4 frames of bonus
        [[nodiscard]] int delayAfterAction(bool isOrderProcessTimerZero) const
        {
            switch (exitSpeed())
            {
                case ReturnExitSpeed::Collision:
                    return 9;
                case ReturnExitSpeed::Low:
                    return 0;
                case ReturnExitSpeed::Medium:
                    return isOrderProcessTimerZero ? -2 : 0;
                case ReturnExitSpeed::High:
                    return isOrderProcessTimerZero ? -4 : 0;
            }
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay, packed) == std::tie(other.arrivalDelay, other.packed);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay, packed) < std::tie(other.arrivalDelay, other.packed);
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(arrivalDelay);
            s.value1b(packed);
        }

        friend std::ostream& operator<< (std::ostream& os, const ReturnArrivalData& data)
        {
            os << data.arrivalDelay;
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
            return os;
        }
    };

    typedef Path<ReturnArrivalData> ReturnPath;
    typedef PathNode<ReturnArrivalData> ReturnPathNode;
}
