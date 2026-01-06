#pragma once

#include "BWAPI/SimulateGatherPathResult.h"

#include "PositionAndVelocity.h"
#include "Path.h"
#include "Geo.h"

#include <cstdint>
#include <algorithm>

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

        // Computes the frame where mining will start
        // If the frame is affected by the given order process timer reset frame, returns an approximated average frame
        [[nodiscard]] int computeActionFrame(std::optional<int> lastResendFrame = std::nullopt,
                                             std::optional<int> orderProcessTimerResetFrame = std::nullopt,
                                             int pathStartFrame = currentFrame) const
        {
            // Compute the arrival frame, using the start frame as either the resend or the path start frame if no resend occurred
            int arrivalFrame = ((lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame) + (int)arrivalDelay();

            // Compute the order process timer value at the start of the frame where the worker is at this node (i.e. where this arrival delay is
            // measured from)
            // If the node is a resend node, the value will be 0 on the next two frames, so we set it to 11 to make the math work
            // If the node is the start of the path, the value will be 0 on the next frame, so we set it to 10 to make the math work
            int orderProcessTimerAtNode = (lastResendFrame.has_value()) ? 11 : 10;

            // Compute the order process timer value at arrival ignoring resets
            int orderProcessTimerAtArrival = orderProcessTimerAtNode - (int)arrivalDelay();
            while (orderProcessTimerAtArrival < 0) orderProcessTimerAtArrival += 9;

            // Compute the action frame ignoring resets
            int actionFrame = arrivalFrame + orderProcessTimerAtArrival;

            // If there is no order process timer reset affecting the result, return now
            if (!orderProcessTimerResetFrame.has_value() || (*orderProcessTimerResetFrame > actionFrame)
                || (lastResendFrame.has_value() && (*orderProcessTimerResetFrame <= *lastResendFrame)))
            {
                return actionFrame;
            }

            // For simplicity we just assume the action frame on average will be 4 frames after the earliest it can be, since we don't need this to
            // be super accurate for training
            return std::max(arrivalFrame, *orderProcessTimerResetFrame) + 4;
        }

        [[nodiscard]] bool isCollisionWithActionAtArrival()
        {
            return collision();
        }

        [[nodiscard]] bool isCollisionWithActionAfterArrival()
        {
            return collision();
        }

        static GatherArrivalData create(unsigned int arrivalDelay,
                                        bool facingTarget,
                                        bool collision,
                                        uint8_t tenDistanceDelta,
                                        const PositionAndVelocity &nextPathStartPosition)
        {
            // Arrival delay values outside the range of 14 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(UINT14_MAX, arrivalDelay);

            // Shift to the left to make room for the boolean bits
            uint16_t packed = (uint16_t)arrivalDelay << 2;

            // We assume we are usually facing the target, so only set the lowest bit if this isn't the case
            if (!facingTarget) packed |= 0b00000001;

            // We set the second-lowest bit if there is a collision
            if (collision) packed |= 0b00000010;

            return GatherArrivalData{packed, tenDistanceDelta, nextPathStartPosition};
        }

        // Populates the members of the struct, except arrivalDelay, from simulated path data
        static GatherArrivalData createFromSimulatedPaths(
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
                const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival,
                BWAPI::Unit patch)
        {
            if (simulatedPathWithActionAtArrival.positions.empty()) return {};

            // The worker is "facing target" if it can turn to face the patch in two frames
            // We check both paths to capture cases where the worker turns while waiting to perform its action
            // This does mean that in some cases we could use a path as long as we are sure the action will occur at arrival, but
            // such paths are rare so we don't want to bother investing the extra data storage
            auto isFacingTarget = [&](const BWAPI::ExactPosition &position)
            {
                auto vectorToPatch = patch->getPosition() - position.pos();
                auto angleDiff = Geo::BWAngleDiff(position.heading, Geo::BWDirection(vectorToPatch));
                return (angleDiff <= 2 * BWAPI::UnitTypes::Protoss_Probe.turnRadius());
            };
            bool facingTarget = isFacingTarget(simulatedPathWithActionAtArrival.actionPosition)
                    && isFacingTarget(simulatedPathWithActionAfterArrival.actionPosition);

            // Find the index of the first position that is 10 distance from the patch
            int i = 0;
            for (auto it = simulatedPathWithActionAtArrival.positions.rbegin();
                it != simulatedPathWithActionAtArrival.positions.rend();
                it++)
            {
                auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                    it->pos(),
                                                    BWAPI::UnitTypes::Resource_Mineral_Field,
                                                    patch->getPosition());
                if (dist > 10) break;
                i++;
            }

            return create(simulatedPathWithActionAtArrival.positions.size(),
                          facingTarget,
                          simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath == 0
                            || simulatedPathWithActionAfterArrival.squaredSpeedEightFramesAlongNextPath == 0,
                          (uint8_t)std::min(i, 255),
                          PositionAndVelocity{simulatedPathWithActionAtArrival.nextPathStartPosition});
        }

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
}
