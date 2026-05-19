#pragma once

#include "BWAPI/SimulateGatherPathResult.h"

#include "InitialWorkerComputePathResult.h"
#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>
#include <algorithm>

#define UINT7_MAX 128U

extern int currentFrame;

namespace MiningOptimizationTraining
{
    int returnExitSpeedToDelay(uint8_t exitSpeed);

    /*
     * Stores the arrival data we need to track for return paths.
     *
     * Arrival delay: the number of frames to arrival at the depot
     * Collision: whether the worker collides with the depot if delivery happened after arrival
     * Exit speed: the exit speed from the depot if delivery happened at arrival
     * Facing depot: whether the worker correctly paths to face the depot, or it does a weird rotation before delivering
     * Gather path start position: the position and velocity of the worker at the start of the gather path, for both delivery at and after arrival
     *
     * We patch the extra data into the arrival delay, since we don't need the full 16 bits.
     */
    struct ReturnArrivalData
    {
        uint8_t arrivalDelay;
        bool collisionDeliveryAfterArrival;
        uint8_t exitSpeedDeliveryAtArrival;
        bool facingDepot;
        PositionAndVelocity nextPathStartPositionDeliveryAtArrival;
        PositionAndVelocity nextPathStartPositionDeliveryAfterArrival;

        void setArrivalDelay(unsigned int value)
        {
            // Arrival delay values outside the range of 7 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(UINT7_MAX, value);
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay,
                            collisionDeliveryAfterArrival,
                            exitSpeedDeliveryAtArrival,
                            facingDepot,
                            nextPathStartPositionDeliveryAtArrival,
                            nextPathStartPositionDeliveryAfterArrival)
                   == std::tie(other.arrivalDelay,
                               other.collisionDeliveryAfterArrival,
                               other.exitSpeedDeliveryAtArrival,
                               other.facingDepot,
                               other.nextPathStartPositionDeliveryAtArrival,
                               other.nextPathStartPositionDeliveryAfterArrival);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay,
                            collisionDeliveryAfterArrival,
                            exitSpeedDeliveryAtArrival,
                            facingDepot,
                            nextPathStartPositionDeliveryAtArrival,
                            nextPathStartPositionDeliveryAfterArrival)
                   < std::tie(other.arrivalDelay,
                               other.collisionDeliveryAfterArrival,
                               other.exitSpeedDeliveryAtArrival,
                               other.facingDepot,
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
            return exitSpeedDeliveryAtArrival == 0;
        }

        [[nodiscard]] bool isCollisionWithActionAfterArrival()
        {
            return collisionDeliveryAfterArrival;
        }

        // Populates the members of the struct, except arrivalDelay, from simulated path data
        static ReturnArrivalData createFromSimulatedPaths(
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival);

        static uint8_t pack(const uint8_t base, const bool extra);
        static void unpack(const uint8_t packed, uint8_t &base, bool &extra);

        template <typename S>
        void serialize(S& s)
        {
            uint8_t first = pack(arrivalDelay, collisionDeliveryAfterArrival);
            s.value1b(first);
            unpack(first, arrivalDelay, collisionDeliveryAfterArrival);

            uint8_t second = pack(exitSpeedDeliveryAtArrival, facingDepot);
            s.value1b(second);
            unpack(second, exitSpeedDeliveryAtArrival, facingDepot);

            s.object(nextPathStartPositionDeliveryAtArrival);
            s.object(nextPathStartPositionDeliveryAfterArrival);
        }

        friend std::ostream& operator<< (std::ostream& os, const ReturnArrivalData& data)
        {
            os << data.arrivalDelay;
            os << "-" << data.exitSpeedDeliveryAtArrival;
            if (data.collisionDeliveryAfterArrival) os << "[c]";
            if (!data.facingDepot) os << "[!f]";
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
    // This is very similar to the above, with the main difference being that the next positions are exact, since we know the starting subpixels
    struct InitialWorkerReturnArrivalData
    {
        uint8_t arrivalDelay;
        bool collisionDeliveryAfterArrival;
        uint8_t exitSpeedDeliveryAtArrival;
        bool facingDepot;
        BWAPI::ExactPosition nextPathStartPositionDeliveryAtArrival;
        BWAPI::ExactPosition nextPathStartPositionDeliveryAfterArrival;

        bool operator==(const InitialWorkerReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay,
                            collisionDeliveryAfterArrival,
                            exitSpeedDeliveryAtArrival,
                            facingDepot,
                            nextPathStartPositionDeliveryAtArrival,
                            nextPathStartPositionDeliveryAfterArrival)
                   == std::tie(other.arrivalDelay,
                               other.collisionDeliveryAfterArrival,
                               other.exitSpeedDeliveryAtArrival,
                               other.facingDepot,
                               other.nextPathStartPositionDeliveryAtArrival,
                               other.nextPathStartPositionDeliveryAfterArrival);
        }

        bool operator<(const InitialWorkerReturnArrivalData &other) const
        {
            return std::tie(arrivalDelay,
                            collisionDeliveryAfterArrival,
                            exitSpeedDeliveryAtArrival,
                            facingDepot,
                            nextPathStartPositionDeliveryAtArrival,
                            nextPathStartPositionDeliveryAfterArrival)
                   < std::tie(other.arrivalDelay,
                               other.collisionDeliveryAfterArrival,
                               other.exitSpeedDeliveryAtArrival,
                               other.facingDepot,
                               other.nextPathStartPositionDeliveryAtArrival,
                               other.nextPathStartPositionDeliveryAfterArrival);
        }

        // Computes the possible action frames, delays, next path starting positions, and whether action happens on arrival for this path
        std::vector<InitialWorkerComputePathResult> computePathResult(
                int pathStartFrame,
                bool pathStartsWithGatherCommand,
                std::optional<int> lastResendFrame,
                const auto &orderProcessTimerResetValues) const
        {
            // The reference frame (where we know the order process timer value) is either the path start or the last resend
            int referenceFrame = (lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame;

            // The arrival frame adds the delay from here
            int arrivalFrame = referenceFrame + arrivalDelay;

            // Set the reference frame and order process timer to where we know the order process timer value
            // The order process timer value here is the value at the start of the frame
            int initialOrderProcessTimer;
            if (lastResendFrame)
            {
                // When we have a resend, the order process timer stays at 0 for an extra frame, so we fast-forward a couple of frames
                referenceFrame += 2;
                initialOrderProcessTimer = 8;
            }
            else
            {
                // At path start the value is 0 because we just gained minerals, but we need to rewind one frame
                referenceFrame--;
                initialOrderProcessTimer = 0;
            }

            // Run the order process timer cycle for each reset value until action and record the results
            std::vector<InitialWorkerComputePathResult> results;
            for (auto resetValue : orderProcessTimerResetValues)
            {
                int frame = referenceFrame;
                int orderProcessTimer = initialOrderProcessTimer;
                bool orderProcessTimerResets = false;
                while (true)
                {
                    if (frame == 158 || frame == 308)
                    {
                        orderProcessTimer = resetValue;
                        orderProcessTimerResets = true;
                    }

                    // Delivery at arrival
                    if (orderProcessTimer == 0 && frame == arrivalFrame)
                    {
                        int delay = returnExitSpeedToDelay(exitSpeedDeliveryAtArrival);
                        results.emplace_back(arrivalFrame,
                                             frame,
                                             true,
                                             delay,
                                             nextPathStartPositionDeliveryAtArrival,
                                             orderProcessTimerResets ? (std::optional<int>)resetValue : std::nullopt);
                        break;
                    }

                    // Delivery after arrival
                    if (orderProcessTimer == 0 && frame > arrivalFrame)
                    {
                        results.emplace_back(arrivalFrame,
                                             frame,
                                             false,
                                             collisionDeliveryAfterArrival ? 9 : 0,
                                             nextPathStartPositionDeliveryAfterArrival,
                                             orderProcessTimerResets ? (std::optional<int>)resetValue : std::nullopt);
                        break;
                    }

                    orderProcessTimer--;
                    if (orderProcessTimer < 0) orderProcessTimer = 8;

                    frame++;
                }
                if (!orderProcessTimerResets) return results;
            }

            return results;
        }

        static InitialWorkerReturnArrivalData createFromSimulatedPath(const BWAPI::SimulateGatherPathResult &simulatedPathDeliveryAtArrival,
                                                                      const BWAPI::SimulateGatherPathResult &simulatedPathDeliveryAfterArrival);

        template <typename S>
        void serialize(S& s) {
            uint8_t first = ReturnArrivalData::pack(arrivalDelay, collisionDeliveryAfterArrival);
            s.value1b(first);
            ReturnArrivalData::unpack(first, arrivalDelay, collisionDeliveryAfterArrival);

            uint8_t second = ReturnArrivalData::pack(exitSpeedDeliveryAtArrival, facingDepot);
            s.value1b(second);
            ReturnArrivalData::unpack(second, exitSpeedDeliveryAtArrival, facingDepot);

            s.object(nextPathStartPositionDeliveryAtArrival);
            s.object(nextPathStartPositionDeliveryAfterArrival);
        }
    };

    typedef InitialWorkerPathNode<InitialWorkerReturnArrivalData> InitialWorkerReturnPathNode;
}
