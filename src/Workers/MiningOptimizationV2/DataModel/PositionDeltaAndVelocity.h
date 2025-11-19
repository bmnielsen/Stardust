#pragma once

#include "Common.h"

#include <cstdint>

namespace MiningOptimizationV2
{
    struct PositionDeltaAndVelocity
    {
        uint8_t packed;     // Packed x and y deltas and whether this struct needs heading and velocity

        int8_t heading;     // The heading in BW representation (1/256th of a circle)
        int16_t velocityX;  // X velocity, in full BW precision, but cut down to 16 bit (since it is only -5 to +5 for workers, plus 8 bit fractional)
        int16_t velocityY;  // Y velocity, in full BW precision, but cut down to 16 bit (since it is only -5 to +5 for workers, plus 8 bit fractional)

        PositionDeltaAndVelocity()
                : packed(0)
                , heading(0)
                , velocityX(0)
                , velocityY(0)
        {}

        PositionDeltaAndVelocity(uint8_t packed, int8_t heading, int16_t velocityX, int16_t velocityY)
                : packed(packed)
                , heading(heading)
                , velocityX(velocityX)
                , velocityY(velocityY)
        {}

        // Index of the position delta into the position delta vector
        [[nodiscard]] uint8_t positionDeltaIndex() const
        {
            // Index is stored in the upper 7 bits, so shift one right and return
            return packed >> 1;
        }

        // Whether this position delta requires using the heading and velocity to differentiate it from its peers
        [[nodiscard]] bool requiresHeadingAndVelocity() const
        {
            // Lowest bit is set if heading and velocity are required
            return (packed & 0b00000001) == 1;
        }

        bool operator==(const PositionDeltaAndVelocity &other) const
        {
            // Packed data must match
            if (packed != other.packed) return false;

            // If heading and velocity don't matter, they are equal
            if (!requiresHeadingAndVelocity()) return true;

            // Otherwise we need to compare the heading and velocity also
            return std::tie(heading, velocityX, velocityY) ==
                   std::tie(other.heading, other.velocityX, other.velocityY);
        }

        template <typename S>
        void serialize(S& s) {
            s.value1b(packed);

            if (requiresHeadingAndVelocity())
            {
                s.value1b(heading);
                s.value2b(velocityX);
                s.value2b(velocityY);
            }
        }
    };
}
