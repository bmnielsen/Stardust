#pragma once

#include "BWAPI/SimulateGatherPathResult.h"

#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>
#include <algorithm>

#define UINT13_MAX 8191U

extern int currentFrame;

namespace MiningOptimizationTraining
{
    enum class ReturnExitSpeed:uint16_t
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
     * Collision: whether the worker collides with the depot if delivery happened after arrival
     * Exit speed: the exit speed from the depot if delivery happened at arrival
     * Gather path start position: the position and velocity of the worker at the start of the gather path, for both delivery at and after arrival
     *
     * We patch the extra data into the arrival delay, since we don't need the full 16 bits.
     */
    struct ReturnArrivalData
    {
        uint16_t packed = UINT16_MAX;
        PositionAndVelocity nextPathStartPositionDeliveryAtArrival;
        PositionAndVelocity nextPathStartPositionDeliveryAfterArrival;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 13 bits, so shift three right and return
            return packed >> 3;
        }

        // The exit speed of the worker from the depot back towards the patch with delivery at arrival
        [[nodiscard]] ReturnExitSpeed exitSpeed() const
        {
            // Exit speed is stored in the lowest two bits
            return (ReturnExitSpeed)(packed & 0b0000000000000011);
        }

        // Whether there was a collision with delivery after arrival
        [[nodiscard]] bool collision() const
        {
            // Third-lowest bit is set if the worker collided
            return (packed & 0b0000000000000100) == 0b0000000000000100;
        }

        void setArrivalDelay(unsigned int arrivalDelay)
        {
            // Arrival delay values outside the range of 14 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(UINT13_MAX, arrivalDelay);

            packed = ((uint16_t)arrivalDelay << 3) + (packed & 0b0000000000000111);
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packed,
                            nextPathStartPositionDeliveryAtArrival,
                            nextPathStartPositionDeliveryAfterArrival)
                            == std::tie(other.packed,
                                        other.nextPathStartPositionDeliveryAtArrival,
                                        other.nextPathStartPositionDeliveryAfterArrival);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed,
                            nextPathStartPositionDeliveryAtArrival,
                            nextPathStartPositionDeliveryAfterArrival)
                   < std::tie(other.packed,
                               other.nextPathStartPositionDeliveryAtArrival,
                               other.nextPathStartPositionDeliveryAfterArrival);
        }

        // Computes the frame where resources will be delivered
        // If the frame is affected by the given order process timer reset frame, returns an approximated average frame
        [[nodiscard]] std::pair<int, int> computeActionFrame(int pathStartFrame,
                                                             std::optional<int> lastResendFrame,
                                                             std::optional<int> orderProcessTimerResetFrame) const;

        [[nodiscard]] bool isCollisionWithActionAtArrival()
        {
            return exitSpeed() == ReturnExitSpeed::Collision;
        }

        [[nodiscard]] bool isCollisionWithActionAfterArrival()
        {
            return collision();
        }

        static ReturnArrivalData create(unsigned int arrivalDelay,
                                        ReturnExitSpeed exitSpeed,
                                        bool collision,
                                        PositionAndVelocity nextPathStartPositionDeliveryAtArrival,
                                        PositionAndVelocity nextPathStartPositionDeliveryAfterArrival);

        // Populates the members of the struct, except arrivalDelay, from simulated path data
        static ReturnArrivalData createFromSimulatedPaths(
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival);

        template <typename S>
        void serialize(S& s) {
            s.value2b(packed);
            s.object(nextPathStartPositionDeliveryAtArrival);
            s.object(nextPathStartPositionDeliveryAfterArrival);
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
            if (data.collision())
            {
                os << "{c}";
            }
            return os;
        }

    private:
        // Returns the delay (e.g. caused by collision) after the action
        // Can be negative if there is an exit speed bonus
        [[nodiscard]] int delayAfterAction(int orderProcessTimerAtArrival) const;
    };

    typedef Path<ReturnArrivalData> ReturnPath;
    typedef PathNode<ReturnArrivalData> ReturnPathNode;

    // Stores the arrival data for an initial worker return path
    // This is similar to the above, with the following differences:
    // - We don't need to store exit speeds since we can just look up the next path
    // - The next path start position is an exact position since we know the starting subpixels
    struct InitialWorkerReturnArrivalData
    {
        uint16_t arrivalDelay = UINT16_MAX;
        BWAPI::ExactPosition nextPathStartPosition;

        bool operator==(const InitialWorkerReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay, nextPathStartPosition) ==
                   std::tie(other.arrivalDelay, other.nextPathStartPosition);
        }

        bool operator<(const InitialWorkerReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay, nextPathStartPosition) <
                   std::tie(other.arrivalDelay, other.nextPathStartPosition);
        }

        static InitialWorkerReturnArrivalData createFromSimulatedPath(const BWAPI::SimulateGatherPathResult &simulatedPath);

        template <typename S>
        void serialize(S& s) {
            s.value2b(arrivalDelay);
            s.object(nextPathStartPosition);
        }
    };

    typedef InitialWorkerPathNode<InitialWorkerReturnArrivalData> InitialWorkerReturnPathNode;
}
