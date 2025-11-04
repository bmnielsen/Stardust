#pragma once

#include <cstdint>
#include "MyWorker.h"
#include <BWAPI/ExactPosition.h>

namespace MiningOptimizationTraining
{
    struct PositionAndVelocity
    {
        uint16_t x;         // X pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        uint16_t y;         // Y pixel position, between 0 and 8191 for the default maximum map size of 256 tiles
        int8_t heading;     // The heading in BW representation (1/256th of a circle)
        int8_t velocityX;   // 8-bit integer representation of the unit's speed on the X axis
        int8_t velocityY;   // 8-bit integer representation of the unit's speed on the Y axis

        PositionAndVelocity()
                : x(0)
                , y(0)
                , heading(0)
                , velocityX(0)
                , velocityY(0)
        {}

        PositionAndVelocity(uint16_t x, uint16_t y, int8_t heading, int8_t velocityX, int8_t velocityY)
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
                , velocityX(MyWorkerImpl::to8bSpeed(unit->getVelocityX()))
                , velocityY(MyWorkerImpl::to8bSpeed(unit->getVelocityY()))
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
            s.value1b(velocityX);
            s.value1b(velocityY);
        }
    };
}

template <>
struct std::hash<MiningOptimizationTraining::PositionAndVelocity>
{
    std::size_t operator()(const MiningOptimizationTraining::PositionAndVelocity& pos) const
    {
        // As this is only intended for use in std::unordered_map, hash quality is not important, so we just pack everything into two ints and XOR
        uint32_t xy = (pos.x << 16) + pos.y;
        uint32_t velocityAndHeading = (unsigned char)pos.heading + ((uint8_t)pos.velocityX << 16) + ((uint8_t)pos.velocityY << 8);
        return xy ^ velocityAndHeading;
    }
};
