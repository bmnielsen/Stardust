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

    // Stores the arrival data for an initial worker gather path
    // This is similar to the above, with the following differences:
    // - We don't bother packing as aggressively since we have a limited data set
    // - We don't need to store collision since we can just look up the next path
    // - The next path start position is an exact position since we know the starting subpixels
    struct InitialWorkerGatherArrivalData
    {
        uint16_t arrivalDelay = UINT16_MAX;
        bool collision = true;
        BWAPI::ExactPosition nextPathStartPosition;

        bool operator==(const InitialWorkerGatherArrivalData &other) const
        {
            return std::tie(arrivalDelay, collision, nextPathStartPosition) ==
                   std::tie(other.arrivalDelay, other.collision, other.nextPathStartPosition);
        }

        bool operator<(const InitialWorkerGatherArrivalData &other) const
        {
            return std::tie(arrivalDelay, collision, nextPathStartPosition) <
                   std::tie(other.arrivalDelay, other.collision, other.nextPathStartPosition);
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(arrivalDelay);
            s.value1b(collision);
            s.object(nextPathStartPosition);
        }
    };

    typedef InitialWorkerPathNode<InitialWorkerGatherArrivalData> InitialWorkerGatherPathNode;
}
