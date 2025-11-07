#pragma once

#include "PositionAndVelocity.h"
#include "Path.h"
#include "Geo.h"

#include <cstdint>
#include <algorithm>

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
     * As we don't care about arrival delays above 63, we pack the arrival delay, facing target, and collision fields into one 8-bit value for
     * efficient serialization.
     */
    struct GatherArrivalData
    {
        uint8_t packed = UINT8_MAX;
        PositionAndVelocity returnPathStartPosition;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 6 bits, so shift two right and return
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
            // Arrival delay values outside the range of 6 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(63U, arrivalDelay);

            packed = ((uint8_t)arrivalDelay << 2) + (packed & 0b00000011);
        }

        bool operator==(const GatherArrivalData &other) const
        {
            return std::tie(packed, returnPathStartPosition) == std::tie(other.packed, other.returnPathStartPosition);
        }

        bool operator<(const GatherArrivalData &other) const
        {
            return std::tie(packed, returnPathStartPosition) < std::tie(other.packed, other.returnPathStartPosition);
        }

        static GatherArrivalData create(unsigned int arrivalDelay,
                                        bool facingTarget,
                                        bool collision,
                                        const PositionAndVelocity &returnPathStartPosition)
        {
            // Arrival delay values outside the range of 6 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(63U, arrivalDelay);

            // Shift to the left to make room for the boolean bits
            uint8_t packed = (uint8_t)arrivalDelay << 2;

            // We assume we are usually facing the target, so only set the lowest bit if this isn't the case
            if (!facingTarget) packed |= 0b00000001;

            // We set the second-lowest bit if there is a collision
            if (collision) packed |= 0b00000010;

            return GatherArrivalData{packed, returnPathStartPosition};
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

            return create(63, facingTarget, std::get<2>(simulatedPath) == 0, PositionAndVelocity{std::get<1>(simulatedPath)});
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(packed);
            s.object(returnPathStartPosition);
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
            return data.packed ^ std::hash<MiningOptimizationTraining::PositionAndVelocity>()(data.returnPathStartPosition);
        }
    };
}
