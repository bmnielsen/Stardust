#pragma once

#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>
#include <algorithm>

#define UINT14_MAX 16383U

namespace MiningOptimizationTraining
{
    enum class ReturnExitSpeed:uint16_t
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
        uint16_t packed = UINT8_MAX;
        PositionAndVelocity nextPathStartPosition;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 14 bits, so shift two right and return
            return packed >> 2;
        }

        // The exit speed of the worker from the depot back towards the patch
        [[nodiscard]] ReturnExitSpeed exitSpeed() const
        {
            // Exit speed is stored in the lowest two bits
            return (ReturnExitSpeed)(packed & 0b00000011);
        }

        void setArrivalDelay(unsigned int arrivalDelay)
        {
            // Arrival delay values outside the range of 14 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(UINT14_MAX, arrivalDelay);

            packed = ((uint16_t)arrivalDelay << 2) + (packed & 0b00000011);
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathStartPosition) == std::tie(other.packed, other.nextPathStartPosition);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathStartPosition) < std::tie(other.packed, other.nextPathStartPosition);
        }

        // Calculates the full delay from the start of the path to when minerals will be delivered, assuming no order process timer resets occur,
        // along with the penalty or benefit from exit speed
        [[nodiscard]] std::pair<unsigned int, int> calculateFullDelay(unsigned int lastResendDistanceFromPathStart) const
        {
            // Compute the order process timer value at the start of the frame where the worker is at this node (i.e. where this arrival delay is
            // measured from)
            // If the node is the start of the path, the value is 0 since the worker just gained minerals
            // If the node is a resend node, the value will be 0 on the next frame, so we set it to 10 to make the math work
            int orderProcessTimerAtNode = (lastResendDistanceFromPathStart == 0) ? 0 : 10;

            // Compute the order process timer value at the arrival frame
            // In the case of a resend, the order timer is 0 at the start of the next frame, so we set it to 9 on this frame for the math to work out

            // Compute the order process timer value at the arrival frame
            // We do this by subtracting and then cycling forward
            int orderProcessTimerAtArrival = orderProcessTimerAtNode - (int)arrivalDelay();
            while (orderProcessTimerAtArrival < 0) orderProcessTimerAtArrival += 9;

            // Compute the exit speed adjustment
            // Collisions add a full order process timer cycle
            // Low exit speed, also applicable if the order process timer is nonzero, is the standard and doesn't modify the timing
            // Medium exit speed saves two frames
            // High exit speed saves four frames
            int exitSpeedAdjustment = 0;
            switch (exitSpeed())
            {
                case ReturnExitSpeed::Collision:
                    exitSpeedAdjustment = 9;
                    break;
                case ReturnExitSpeed::Medium:
                    if (orderProcessTimerAtArrival == 0) exitSpeedAdjustment = -2;
                    break;
                case ReturnExitSpeed::High:
                    if (orderProcessTimerAtArrival == 0) exitSpeedAdjustment = -4;
                    break;
                default:
                    break;
            }

            // Put everything together
            return std::make_pair(
                    lastResendDistanceFromPathStart + arrivalDelay() + orderProcessTimerAtArrival,
                    exitSpeedAdjustment);
        }

        static ReturnArrivalData create(unsigned int arrivalDelay, ReturnExitSpeed exitSpeed, PositionAndVelocity nextPathStartPosition)
        {
            // Arrival delay values outside the range of 14 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(UINT14_MAX, arrivalDelay);

            // Shift to the left to make room for the exit speed
            uint16_t packed = (uint16_t)arrivalDelay << 2;

            // Add the exit speed
            packed += (uint16_t)exitSpeed;

            return ReturnArrivalData{packed, std::move(nextPathStartPosition)};
        }

        // Populates the members of the struct, except arrivalDelay, from simulated path data
        static ReturnArrivalData createFromSimulatedPath(
                const std::tuple<std::vector<BWAPI::ExactPosition>, BWAPI::ExactPosition, uint64_t> &simulatedPath)
        {
            // The return exit speed is bucketed based on the squared speed
            // The max speed of a worker is 5 pixels per frame, which corresponds to 5*256=1280 subpixels per frame or 1638400 squared
            ReturnExitSpeed returnExitSpeed;
            if (std::get<2>(simulatedPath) > 1048576) // 80% of top speed
            {
                returnExitSpeed = ReturnExitSpeed::High;
            }
            else if (std::get<2>(simulatedPath) > 409600) // 50% of top speed
            {
                returnExitSpeed = ReturnExitSpeed::Medium;
            }
            else if (std::get<2>(simulatedPath) == 0)
            {
                returnExitSpeed = ReturnExitSpeed::Collision;
            }
            else
            {
                returnExitSpeed = ReturnExitSpeed::Low;
            }

            return create(std::get<0>(simulatedPath).size(), returnExitSpeed, PositionAndVelocity{std::get<1>(simulatedPath)});
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(packed);
            s.object(nextPathStartPosition);
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
            return data.packed ^ std::hash<MiningOptimizationTraining::PositionAndVelocity>()(data.nextPathStartPosition);
        }
    };
}
