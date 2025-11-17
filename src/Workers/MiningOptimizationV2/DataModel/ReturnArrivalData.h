#pragma once

#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>
#include <algorithm>

#define UINT6_MAX 63U

namespace MiningOptimizationV2
{
    enum class ReturnExitSpeed:uint8_t
    {
        Collision,  // The worker collided with the depot when trying to leave it
        Low,        // The worker stopped at the depot and will therefore accelerate slowly towards the patch
        Medium,     // The worker maintained some speed after delivery
        High,       // The worker maintained a great deal of speed after delivery
    };

    std::ostream& operator<<(std::ostream& os, const ReturnExitSpeed exitSpeed);

    /*
     * Stores the arrival data we need to track for return paths.
     *
     * Arrival delay: the number of frames to arrival at the depot
     * Exit speed: the exit speed from the depot
     * Gather path start position: the position and velocity of the worker at the start of the gather path
     *
     * We patch the exit speed data into the arrival delay, since we don't need the full 16 bits.
     */
    struct ReturnArrivalData
    {
        uint8_t packed = UINT8_MAX;
        uint8_t nextPathLength = UINT8_MAX;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 6 bits, so shift two right and return
            return packed >> 2;
        }

        // The exit speed of the worker from the depot back towards the patch
        [[nodiscard]] ReturnExitSpeed exitSpeed() const
        {
            // Exit speed is stored in the lowest two bits
            return (ReturnExitSpeed)(packed & 0b00000011);
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathLength) == std::tie(other.packed, other.nextPathLength);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathLength) < std::tie(other.packed, other.nextPathLength);
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(packed);
            s.value1b(nextPathLength);
        }

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
            return os;
        }
    };

    typedef Path<ReturnArrivalData> ReturnPath;
    typedef PathNode<ReturnArrivalData> ReturnPathNode;
}
