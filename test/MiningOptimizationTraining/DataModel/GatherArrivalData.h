#pragma once

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
     * Return path start position: the position and velocity of the worker at the start of the return path
     *
     * We patch the collision and facing target data into the arrival delay, since we don't need the full 16 bits.
     */
    struct GatherArrivalData
    {
        uint16_t packed = UINT8_MAX;
        PositionAndVelocity nextPathStartPosition;

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
            return std::tie(packed, nextPathStartPosition) == std::tie(other.packed, other.nextPathStartPosition);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(packed, nextPathStartPosition) < std::tie(other.packed, other.nextPathStartPosition);
        }

        // Calculates the full delay from the start of the path to when mining will start, assuming no order process timer resets occur, along
        // with the penalty from not facing patch or colliding with it
        [[nodiscard]] std::pair<unsigned int, int> calculateFullDelay(unsigned int lastResendDistanceFromPathStart) const
        {
            // Compute the order process timer value at the start of the frame where the worker is at this node (i.e. where this arrival delay is
            // measured from)
            // If the node is the start of the path, the value will be 0 on the next frame, so we set it to 11 to make the math work
            // If the node is a resend node, the value will be 0 on the next two frames, so we set it to 11 to make the math work
            int orderProcessTimerAtNode = (lastResendDistanceFromPathStart == 0) ? 10 : 11;

            // Compute the order process timer value at the arrival frame
            // We do this by subtracting and then cycling forward
            int orderProcessTimerAtArrival = orderProcessTimerAtNode - (int)arrivalDelay();
            while (orderProcessTimerAtArrival < 0) orderProcessTimerAtArrival += 9;

            // Put everything together
            // Both having to turn to face the patch and colliding with it incur an extra order process timer cycle of delay after mining starts/ends
            return std::make_pair(
                    lastResendDistanceFromPathStart + arrivalDelay() + orderProcessTimerAtArrival,
                    (!facingTarget() ? 9 : 0) + (collision() ? 9 : 0));
        }

        static GatherArrivalData create(unsigned int arrivalDelay,
                                        bool facingTarget,
                                        bool collision,
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

            return GatherArrivalData{packed, nextPathStartPosition};
        }

        // Populates the members of the struct, except arrivalDelay, from simulated path data
        static GatherArrivalData createFromSimulatedPath(
                const std::tuple<std::vector<BWAPI::ExactPosition>, BWAPI::ExactPosition, uint64_t> &simulatedPath,
                BWAPI::Unit patch)
        {
            auto &positionHistory = std::get<0>(simulatedPath);
            if (positionHistory.empty()) return {};

            // The worker is "facing target" if it can turn to face the patch in two frames
            auto finalWorkerPosition = *positionHistory.rbegin();
            auto vectorToPatch = patch->getPosition() - finalWorkerPosition.pos();
            auto angleDiff = Geo::BWAngleDiff(finalWorkerPosition.heading, Geo::BWDirection(vectorToPatch));
            bool facingTarget = (angleDiff <= 2 * BWAPI::UnitTypes::Protoss_Probe.turnRadius());

            return create(positionHistory.size(), facingTarget, std::get<2>(simulatedPath) == 0, PositionAndVelocity{std::get<1>(simulatedPath)});
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(packed);
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

namespace std {
    template <> struct hash<MiningOptimizationTraining::GatherArrivalData>
    {
        size_t operator()(const MiningOptimizationTraining::GatherArrivalData& data) const
        {
            // As this is only intended for use in std::unordered_map, hash quality is not important
            return data.packed ^ std::hash<MiningOptimizationTraining::PositionAndVelocity>()(data.nextPathStartPosition);
        }
    };
}
