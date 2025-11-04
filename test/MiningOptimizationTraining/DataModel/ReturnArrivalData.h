#pragma once

#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>
#include <algorithm>

namespace MiningOptimizationTraining
{
    enum class ReturnExitSpeed:uint8_t
    {
        Collision,  // The worker collided with the depot when trying to leave it
        Low,        // The worker stopped at the depot and will therefore accelerate slowly towards the patch
        Medium,     // The worker maintained some speed after delivery
        High,       // The worker maintained a great deal of speed after delivery
    };

    /*
     * Stores the arrival data we need to track for return paths.
     *
     * Arrival delay: the number of frames to arrival at the depot
     * Exit speed: the exit speed from the depot
     * Gather path start position: the position and velocity of the worker at the start of the gather path
     *
     * As we don't care about arrival delays above 63, we pack the arrival delay and exit speed into one 8-bit value for efficient serialization.
     */
    struct ReturnArrivalData
    {
        uint8_t packed;
        PositionAndVelocity gatherPathStartPosition;

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
            return std::tie(packed, gatherPathStartPosition) == std::tie(other.packed, other.gatherPathStartPosition);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed, gatherPathStartPosition) < std::tie(other.packed, other.gatherPathStartPosition);
        }

        static ReturnArrivalData create(unsigned int arrivalDelay, ReturnExitSpeed exitSpeed, PositionAndVelocity gatherPathStartPosition)
        {
            // Arrival delay values outside the range of 6 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(63U, arrivalDelay);

            // Shift to the left to make room for the exit speed
            uint8_t packed = (uint8_t)arrivalDelay << 2;

            // Add the exit speed
            packed += (uint8_t)exitSpeed;

            return ReturnArrivalData{packed, std::move(gatherPathStartPosition)};
        }

        // A "null" value that just gives the worst delay possible
        static ReturnArrivalData nullopt()
        {
            return create(63U, ReturnExitSpeed::Collision, {});
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(packed);
            s.object(gatherPathStartPosition);
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

namespace std {
    template <> struct hash<MiningOptimizationTraining::ReturnArrivalData>
    {
        size_t operator()(const MiningOptimizationTraining::ReturnArrivalData& data) const
        {
            // As this is only intended for use in std::unordered_map, hash quality is not important
            return data.packed ^ std::hash<MiningOptimizationTraining::PositionAndVelocity>()(data.gatherPathStartPosition);
        }
    };
}
