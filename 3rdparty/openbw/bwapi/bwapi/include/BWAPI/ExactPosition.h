#pragma once

#include "Position.h"
#include <cinttypes>

namespace BWAPI
{
    // Represents the difference between two exact positions.
    // Does not take the heading into consideration.
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

    // Represents a unit position including the subpixel position and heading.
    // Roughly analogous to the "xy_fp8" type used in OpenBW to represent the exact position of a unit, but simplified, and including the heading.
    // The heading is in internal BW representation, so in units of 1/256th of a circle.
    struct ExactPosition
    {
        uint32_t x;
        uint32_t y;
        int8_t heading;

        ExactPosition(uint32_t x, uint32_t y, int8_t heading) : x(x), y(y), heading(heading) {}

        explicit ExactPosition(const std::tuple<uint32_t, uint32_t, int8_t> &xyh)
            : x(std::get<0>(xyh))
            , y(std::get<1>(xyh))
            , heading(std::get<2>(xyh))
            {}

        Position pos() const
        {
            return {(int)(x >> 8), (int)(y >> 8)};
        }

        bool operator == (const ExactPosition &other) const
        {
            return std::tie(this->x, this->y, this->heading) == std::tie(other.x, other.y, other.heading);
        };

        bool operator != (const ExactPosition &other) const
        {
            return !(*this == other);
        };

        bool operator  < (const ExactPosition &other) const
        {
            return std::tie(this->x, this->y, this->heading) < std::tie(other.x, other.y, other.heading);
        };

        ExactPositionDifference operator - (const ExactPosition &other) const
        {
            int32_t dx = this->x - other.x;
            int32_t dy = this->y - other.y;
            return {dx, dy};
        }

        friend std::ostream &operator << (std::ostream &os, const ExactPosition &pos)
        {
            return os << '(' << pos.x << ',' << pos.y << ')';
        };
    };
}
