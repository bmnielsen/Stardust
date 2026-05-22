#pragma once

#include <cstdint>
#include <BWAPI/Unit.h>
#include <BWAPI/ExactPosition.h>

namespace MiningOptimizationTraining
{
    struct PositionAndVelocity
    {
        uint16_t x;         // X pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        uint16_t y;         // Y pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        int8_t heading;     // The heading in BW representation (1/256th of a circle)
        int32_t velocityX;  // X velocity, in full BW precision
        int32_t velocityY;  // Y velocity, in full BW precision

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

        explicit PositionAndVelocity(const BWAPI::Unit &unit)
                : x((uint16_t)unit->getPosition().x)
                , y((uint16_t)unit->getPosition().y)
                , heading(unit->getExactPosition().heading)
                , velocityX(unit->getExactPosition().velocityX)
                , velocityY(unit->getExactPosition().velocityY)
        {}

        explicit PositionAndVelocity(const BWAPI::ExactPosition &exactPosition)
                : x((uint16_t)exactPosition.pos().x)
                , y((uint16_t)exactPosition.pos().y)
                , heading(exactPosition.heading)
                , velocityX(exactPosition.velocityX)
                , velocityY(exactPosition.velocityY)
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

        operator BWAPI::Position() const
        {
            return BWAPI::Position(x, y);
        }

        template <typename S>
        void serialize(S& s) {
            s.value2b(x);
            s.value2b(y);
            s.value1b(heading);
            s.value4b(velocityX);
            s.value4b(velocityY);
        }

        friend std::ostream& operator<< (std::ostream& os, const PositionAndVelocity& pos)
        {
            os << "(" << pos.x << "," << pos.y << ",h=" << (int)pos.heading << ",dx=" << pos.velocityX << ",dy=" << pos.velocityY << ")";
            return os;
        }
    };
}

template <>
struct std::hash<MiningOptimizationTraining::PositionAndVelocity>
{
    std::size_t operator()(const MiningOptimizationTraining::PositionAndVelocity& pos) const
    {
        // As this is only intended for use in std::unordered_map, hash quality is not important, so we just use XOR
        uint32_t xy = (pos.x << 16) + pos.y;
        return xy ^ (uint32_t)pos.heading ^ (uint32_t)pos.velocityX ^ (uint32_t)pos.velocityY;
    }
};
