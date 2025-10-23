#pragma once

#include <cstdint>
#include <algorithm>

namespace MiningOptimizationTraining
{
    /*
     * Stores the arrival data we need to track.
     * Arrival delay: the number of frames to arrival at the target
     * Facing target: whether the worker is facing its target at arrival
     *
     * As we don't care about arrival delays above 127, we can pack both fields into one 8-bit value for efficient serialization.
     */
    struct ArrivalData
    {
        uint8_t packed;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 7 bits, so shift one right and return
            return packed >> 1;
        }

        // Whether the worker is facing its target at arrival
        [[nodiscard]] bool facingTarget() const
        {
            // Lowest bit is set if the worker is not facing the target
            return !(packed & 1);
        }

        bool operator==(const ArrivalData &other) const
        {
            return packed == other.packed;
        }

        bool operator<(const ArrivalData &other) const
        {
            return packed < other.packed;
        }

        static ArrivalData create(unsigned int arrivalDelay, bool facingTarget)
        {
            // Values outside the range of 7 bits are clamped
            // This is fine since such long arrival delays would never be useful for optimization anyway
            arrivalDelay = std::min(127U, arrivalDelay);

            // Shift to the left to make room for the "facing target" bit
            uint8_t packed = (uint8_t)arrivalDelay << 1;

            // We assume we are usually facing the target, so only set the lowest bit if this isn't the case
            if (!facingTarget) packed |= 0b00000001;

            return ArrivalData{packed};
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(packed);
        }

        friend std::ostream& operator<< (std::ostream& os, const ArrivalData& data)
        {
            os << data.arrivalDelay();
            if (!data.facingTarget())
            {
                os << "!facing";
            }
            return os;
        }
    };
}

namespace std {
    template <> struct hash<MiningOptimizationTraining::ArrivalData>
    {
        size_t operator()(const MiningOptimizationTraining::ArrivalData& data) const
        {
            return data.packed;
        }
    };
}
