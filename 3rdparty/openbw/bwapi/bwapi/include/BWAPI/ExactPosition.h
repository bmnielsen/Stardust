#pragma once

#include "Position.h"
#include <cinttypes>

namespace BWAPI
{
    // Represents the difference between two exact positions.
    // Does not take the heading or velocity into consideration.
    struct ExactPositionDifference
    {
        int32_t x;
        int32_t y;

        bool operator == (const ExactPositionDifference &other) const
        {
            return std::tie(this->x, this->y) == std::tie(other.x, other.y);
        };

        bool operator != (const ExactPositionDifference &other) const
        {
            return !(*this == other);
        };

        bool operator  < (const ExactPositionDifference &other) const
        {
            return std::tie(this->x, this->y) < std::tie(other.x, other.y);
        };

        friend std::ostream &operator << (std::ostream &os, const ExactPositionDifference &pos)
        {
            return os << '(' << pos.x << ',' << pos.y << ')';
        };
    };

    // Represents a unit position, heading and velocity with the full precision used by the BW engine.
    // For x and y positions, this includes 8 bits of subpixel precision.
    // For heading, the representation is 1/256th of a circle.
    // For velocities, the lower 8 bits are the fractional part of the velocity.
    struct ExactPosition
    {
        uint32_t x;
        uint32_t y;
        int8_t heading;
        int32_t velocityX;
        int32_t velocityY;

        ExactPosition(uint32_t x, uint32_t y, int8_t heading, int32_t velocityX, int32_t velocityY)
            : x(x)
            , y(y)
            , heading(heading)
            , velocityX(velocityX)
            , velocityY(velocityY) {}

        Position pos() const
        {
            return {(int)(x >> 8), (int)(y >> 8)};
        }

        bool operator == (const ExactPosition &other) const
        {
            return std::tie(this->x, this->y, this->heading, this->velocityX, this->velocityY)
                == std::tie(other.x, other.y, other.heading, other.velocityX, other.velocityY);
        };

        bool operator != (const ExactPosition &other) const
        {
            return !(*this == other);
        };

        bool operator  < (const ExactPosition &other) const
        {
            return std::tie(this->x, this->y, this->heading, this->velocityX, this->velocityY)
                   < std::tie(other.x, other.y, other.heading, other.velocityX, other.velocityY);
        };

        ExactPositionDifference operator - (const ExactPosition &other) const
        {
            int32_t dx = this->x - other.x;
            int32_t dy = this->y - other.y;
            return {dx, dy};
        }

        friend std::ostream &operator << (std::ostream &os, const ExactPosition &pos)
        {
            return os << '(' << pos.x << ',' << pos.y << ",h=" << (int)pos.heading << ",vx=" << pos.velocityX << ",vy=" << pos.velocityY << ')';
        };
    };
}
