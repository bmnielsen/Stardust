#include "Block.h"

class Block12x5 : public BuildBlock
{
public:
    Block12x5(BWAPI::TilePosition cornerTL, BWAPI::TilePosition powerPylon) : BuildBlock(cornerTL, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 12; }

    [[nodiscard]] int height() const override { return 5; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<BuildBlock> tryCreate(BWAPI::TilePosition tilePos, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tilePos, tileAvailability))
        {
            return std::make_shared<Block12x5>(tilePos, tilePos + BWAPI::TilePosition(4, 0));
        }

        return nullptr;
    }

    bool tilesUsed(BWAPI::TilePosition tilePos, BWAPI::TilePosition size) override
    {
        if (!BuildBlock::tilesUsed(tilePos, size)) return false;

        /* When the right converted pylon is taken, it opens up two more pylons */
        if (tilePos == (cornerTL + BWAPI::TilePosition(6, 0)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(cornerTL + BWAPI::TilePosition(8, 0), true);
            small.emplace_back(cornerTL + BWAPI::TilePosition(10, 0), true);
            removeUsed();
            return true;
        }

        return true;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 0));
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 0));
        small.emplace_back(cornerTL);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(6, 0));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(9, 0));
        large.emplace_back(cornerTL + BWAPI::TilePosition(0, 2));
        large.emplace_back(cornerTL + BWAPI::TilePosition(4, 2));
        large.emplace_back(cornerTL + BWAPI::TilePosition(8, 2));
        medium.emplace_back(cornerTL + BWAPI::TilePosition(1, 2), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(5, 2), true);
        medium.emplace_back(cornerTL + BWAPI::TilePosition(8, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 0), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(0, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(2, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(4, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(6, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(8, 2), true);
        small.emplace_back(cornerTL + BWAPI::TilePosition(10, 2), true);
    }
};