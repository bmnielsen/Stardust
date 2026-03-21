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
    operator BWAPI::TilePosition() const
    {
        return BWAPI::TilePosition(x, y);
    }

    friend std::ostream &operator << (std::ostream &os, const TilePosition &pos)
    {
        return os << '(' << (int)pos.x << ',' << (int)pos.y << ')';
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

template <>
struct std::hash<TilePosition>
{
    std::size_t operator()(const TilePosition& pos) const
    {
        // Only intended for use in std::unordered_map, so hash quality is not important
        return (pos.x << 8) + pos.y;
    }
};
