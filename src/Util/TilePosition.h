#pragma once

#include "BWAPI.h"

struct TilePosition
{
public:
    uint8_t x;
    uint8_t y;

    bool operator == (const TilePosition &pos) const
    {
        return std::tie(this->x, this->y) == std::tie(pos.x, pos.y);
    };
    bool operator != (const TilePosition &pos) const
    {
        return !(*this == pos);
    };
    bool operator  < (const TilePosition &pos) const
    {
        return std::tie(this->x, this->y) < std::tie(pos.x, pos.y);
    };

    friend std::ostream &operator << (std::ostream &os, const TilePosition &pos)
    {
        return os << '(' << pos.x << ',' << pos.y << ')';
    };

    static TilePosition fromBWAPI(BWAPI::TilePosition tile)
    {
        if (tile.x < 0 || tile.x > 255)
        {
            throw std::runtime_error("Failed to construct TilePosition from BWAPI::TilePosition: x out of bounds");
        }
        if (tile.y < 0 || tile.y > 255)
        {
            throw std::runtime_error("Failed to construct TilePosition from BWAPI::TilePosition: y out of bounds");
        }
        return TilePosition{(uint8_t)tile.x, (uint8_t)tile.y};
    }

    template <typename S>
    void serialize(S& s) {
        s.value1b(x);
        s.value1b(y);
    }
};

