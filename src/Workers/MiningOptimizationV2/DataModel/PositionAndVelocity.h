#pragma once

#include "Common.h"

#include <cstdint>

namespace MiningOptimization
{
    struct PositionAndVelocity
    {
        uint16_t x;         // X pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        uint16_t y;         // Y pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        int8_t heading;     // The heading in BW representation (1/256th of a circle)
        int16_t velocityX;  // X velocity, in full BW precision, but cut down to 16 bit (since it is only -5 to +5 for workers, plus 8 bit fractional)
        int16_t velocityY;  // Y velocity, in full BW precision, but cut down to 16 bit (since it is only -5 to +5 for workers, plus 8 bit fractional)

        PositionAndVelocity()
                : x(0)
                , y(0)
                , heading(0)
                , velocityX(0)
                , velocityY(0)
        {}

        PositionAndVelocity(uint16_t x, uint16_t y, int8_t heading, int32_t velocityX, int32_t velocityY)
                : x(x)
                , y(y)
                , heading(heading)
                , velocityX(velocityX)
                , velocityY(velocityY)
        {}

        bool operator==(const PositionAndVelocity &other) const
        {
            return std::tie(x, y, heading, velocityX, velocityY) ==
                   std::tie(other.x, other.y, other.heading, other.velocityX, other.velocityY);
        }

        bool operator<(const PositionAndVelocity &other) const
        {
            return std::tie(x, y, heading, velocityX, velocityY) <
                   std::tie(other.x, other.y, other.heading, other.velocityX, other.velocityY);
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(x);
            s.value2b(y);
            s.value1b(heading);

            // We actually don't need to store the velocity for return path root nodes, since the worker is always stopped there
            // But removing it actually made the compression worse, so there was no benefit and the code is messier
            s.value2b(velocityX);
            s.value2b(velocityY);
        }
    };
}

template <>
struct std::hash<MiningOptimization::PositionAndVelocity>
{
    std::size_t operator()(const MiningOptimization::PositionAndVelocity& pos) const
    {
        // As this is only intended for use in std::unordered_map, hash quality is not important, so we just use XOR
        uint32_t xy = (pos.x << 16) + pos.y;
        return xy ^ (uint32_t)pos.heading ^ (uint32_t)pos.velocityX ^ (uint32_t)pos.velocityY;
    }
};
