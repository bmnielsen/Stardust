#pragma once

#include "TilePosition.h"
#include <cstdint>

namespace MiningOptimization
{
    class CannonPlacement
    {
    public:
        uint8_t cannonCount;
        TilePosition tile;

        bool operator == (const CannonPlacement &other) const
        {
            if (cannonCount == 0 && other.cannonCount == 0) return true;
            return std::tie(this->cannonCount, this->tile) == std::tie(other.cannonCount, other.tile);
        };
        bool operator != (const CannonPlacement &other) const
        {
            return !(*this == other);
        };
        bool operator  < (const CannonPlacement &other) const
        {
            if (cannonCount == 0 || other.cannonCount == 0) return cannonCount < other.cannonCount;
            return std::tie(this->cannonCount, this->tile) < std::tie(other.cannonCount, other.tile);
        };

        friend std::ostream &operator << (std::ostream &os, const CannonPlacement &cannonPlacement)
        {
            if (cannonPlacement.cannonCount == 0)
            {
                return os << "none";
            }
            if (cannonPlacement.cannonCount == 1)
            {
                return os << "1 cannon @ " << cannonPlacement.tile;
            }
            return os << cannonPlacement.cannonCount << " cannons; last @ " << cannonPlacement.tile;
        };

        template <typename S>
        void serialize(S& s) {
            s.value1b(cannonCount);
            if (cannonCount > 0)
            {
                s.object(tile);
            }
        }
    };
}

template <>
struct std::hash<MiningOptimization::CannonPlacement>
{
    std::size_t operator()(const MiningOptimization::CannonPlacement& cannonPlacement) const
    {
        // Only intended for use in std::unordered_map, so hash quality is not important
        if (cannonPlacement.cannonCount == 0) return 0;
        return (cannonPlacement.cannonCount << 16) + (cannonPlacement.tile.x << 8) + cannonPlacement.tile.y;
    }
};
