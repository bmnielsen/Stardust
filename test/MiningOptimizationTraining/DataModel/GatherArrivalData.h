#pragma once

#include "BWAPI/SimulateGatherPathResult.h"

#include "InitialWorkerComputePathResult.h"
#include "PositionAndVelocity.h"
#include "Path.h"
#include "Geo.h"

#include <cstdint>
#include <algorithm>
#include <ranges>

#define UINT14_MAX 16383U

namespace MiningOptimizationTraining
{
    /*
     * Stores the arrival data we need to track for gather paths.
     *
     * Arrival delay: the number of frames to arrival at the target
     * Facing target: whether the worker is facing its target at arrival
     * Collision: whether there was a collision when the worker leaves the target again
     * Ten distance delta: the delta (counted from the end of the path) to the first position within 10 distance of the patch
     * Return path start position: the position and velocity of the worker at the start of the return path
     *
     * We patch the collision and facing target data into the arrival delay, since we don't need the full 16 bits.
     */
    struct GatherArrivalData
    {
        uint16_t packed = UINT16_MAX;
        uint8_t tenDistanceDelta = UINT8_MAX;
        PositionAndVelocity nextPathStartPosition;

        // This is intentionally not included in the key, since we assume it will not change for the same arrival delay
        mutable uint8_t resendAlwaysArrivesDelta = UINT8_MAX;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 14 bits, so shift two right and return
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

        void setArrivalDelay(unsigned int arrivalDelay)
        {
            // Arrival delay values outside the range of 14 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(UINT14_MAX, arrivalDelay);

            packed = ((uint16_t)arrivalDelay << 2) + (packed & 0b00000011);
        }

        // Computes the frame where mining will begin
        // If the frame is affected by the given order process timer reset frame, returns an approximated average frame
        [[nodiscard]] std::pair<int, int> computeActionFrame(int pathStartFrame,
                                                             std::optional<int> lastResendFrame,
                                                             std::optional<int> orderProcessTimerResetFrame) const;

        bool operator==(const GatherArrivalData &other) const
        {
            return std::tie(packed, tenDistanceDelta, nextPathStartPosition) ==
                std::tie(other.packed, other.tenDistanceDelta, other.nextPathStartPosition);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(packed, tenDistanceDelta, nextPathStartPosition) <
                std::tie(other.packed, other.tenDistanceDelta, other.nextPathStartPosition);
        }

        static GatherArrivalData create(unsigned int arrivalDelay,
                                        bool facingTarget,
                                        bool collision,
                                        uint8_t tenDistanceDelta,
                                        const PositionAndVelocity &nextPathStartPosition);

        // Populates the members of the struct, except arrivalDelay, from simulated path data
        static GatherArrivalData createFromSimulatedPaths(
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival,
                BWAPI::Unit patch,
                const BWAPI::ExactPosition &currentPosition);

        template <typename S>
        void serialize(S& s) {
            s.value2b(packed);
            s.value1b(tenDistanceDelta);
            s.value1b(resendAlwaysArrivesDelta);
            s.object(nextPathStartPosition);
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

    // Stores the arrival data for an initial worker gather path
    // This is similar to the above, with the following differences:
    // - We don't bother packing as aggressively since we have a limited data set
    // - We don't need to store collision since we can just look up the next path
    // - The next path start position is an exact position since we know the starting subpixels
    struct InitialWorkerGatherArrivalData
    {
        uint16_t arrivalDelay = UINT16_MAX;
        bool facingPatch = false;
        bool collision = false;
        BWAPI::ExactPosition nextPathStartPosition;

        bool operator==(const InitialWorkerGatherArrivalData &other) const
        {
            return std::tie(arrivalDelay, facingPatch, collision, nextPathStartPosition) ==
                   std::tie(other.arrivalDelay, other.facingPatch, other.collision, other.nextPathStartPosition);
        }

        bool operator<(const InitialWorkerGatherArrivalData &other) const
        {
            return std::tie(arrivalDelay, facingPatch, collision, nextPathStartPosition) <
                   std::tie(other.arrivalDelay, other.facingPatch, other.collision, other.nextPathStartPosition);
        }

        // Computes the possible action frames, delays, and next path starting positions for this path
        // The last part of the tuple is whether the result has action on arrival, which for gather is meaningless so we always return true
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

            // Adjust the reference frame to where we know the order process timer value
            // The order process timer value here is the value at the start of the frame
            // It is in reality in the range 0-8, but always starts with an extra frame at 0 so using 10 makes the math work
            int initialOrderProcessTimer = 10;
            if (lastResendFrame)
            {
                // When we have a resend, the order process timer stays at 0 for two frames, so we increment the reference frame
                referenceFrame++;
            }
            else if (pathStartsWithGatherCommand)
            {
                // When there is no resend, but the path started with a gather command, we adjust the reference frame to account for the gather command
                // and latency
                referenceFrame += 3;
            }
            else
            {
                // Rewind one frame to have the order process timer cycle line up with the start frame
                referenceFrame--;
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
                    if ((frame == 158 || frame == 308) && frame > referenceFrame)
                    {
                        orderProcessTimer = resetValue;
                        orderProcessTimerResets = true;
                    }

                    if (orderProcessTimer == 0 && frame >= arrivalFrame)
                    {
                        results.emplace_back(arrivalFrame,
                                             frame,
                                             true,
                                             collision ? 9 : 0,
                                             nextPathStartPosition,
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

        // Creates the struct from a simulated path
        static InitialWorkerGatherArrivalData createFromSimulatedPath(const BWAPI::SimulateGatherPathResult &simulatedPath, BWAPI::Unit patch);

        template <typename S>
        void serialize(S& s) {
            uint16_t packed = ((uint16_t)std::min(UINT14_MAX, (unsigned int)arrivalDelay)) << 2;
            if (!facingPatch) packed |= 0b00000001;
            if (collision) packed |= 0b00000010;

            s.value2b(packed);
            s.object(nextPathStartPosition);

            arrivalDelay = packed >> 2;
            facingPatch = (packed & 0b00000001) == 0;
            collision = (packed & 0b00000010) == 0b00000010;
        }
    };

    typedef InitialWorkerPathNode<InitialWorkerGatherArrivalData> InitialWorkerGatherPathNode;
}
