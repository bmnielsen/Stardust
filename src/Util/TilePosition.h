#pragma once

#include "BWAPI.h"

struct TilePosition
{
public:
    uint8_t x;
    uint8_t y;

    bool operator == (const TilePosition &pt) const
    {
        return std::tie(this->x, this->y) == std::tie(pt.x, pt.y);
    };
    bool operator != (const TilePosition &pt) const
    {
        return !(*this == pt);
    };
    bool operator  < (const TilePosition &pt) const
    {
        return std::tie(this->x, this->y) < std::tie(pt.x, pt.y);
    };

    friend std::ostream &operator << (std::ostream &os, const TilePosition &pt)
    {
        return os << '(' << (int)pt.x << ',' << (int)pt.y << ')';
    };

    static TilePosition fromBWAPI(BWAPI::TilePosition tilePos)
    {
        if (tilePos.x < 0 || tilePos.x > 255)
        {
            throw std::runtime_error("Failed to construct TilePosition from BWAPI::TilePosition: x out of bounds");
        }
        if (tilePos.y < 0 || tilePos.y > 255)
        {
            throw std::runtime_error("Failed to construct TilePosition from BWAPI::TilePosition: y out of bounds");
        }
        return TilePosition{(uint8_t)tilePos.x, (uint8_t)tilePos.y};
    }

    template <typename S>
    void serialize(S& s) {
        s.value1b(x);
        s.value1b(y);
    }
};

