#include "Block.h"

class Block16x5 : public Block
{
public:
    Block16x5(BWAPI::TilePosition topLeft, BWAPI::TilePosition powerPylon) : Block(topLeft, powerPylon) { placeLocations(); }

    [[nodiscard]] int width() const override { return 16; }

    [[nodiscard]] int height() const override { return 5; }

    [[nodiscard]] bool allowTopEdge() const override { return false; }

    [[nodiscard]] bool allowCorner() const override { return false; }

    std::shared_ptr<Block> tryCreate(BWAPI::TilePosition tile, std::vector<unsigned int> &tileAvailability) override
    {
        if (place(tile, tileAvailability))
        {
            return std::make_shared<Block16x5>(tile, tile + BWAPI::TilePosition(8, 0));
        }

        return nullptr;
    }

    bool tilesUsed(BWAPI::TilePosition tile, BWAPI::TilePosition size) override
    {
        if (!Block::tilesUsed(tile, size)) return false;

        // When the right converted pylon is taken, it opens up two more pylons
        if (tile == (topLeft + BWAPI::TilePosition(10, 0)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(12, 0), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(14, 0), true);
            removeUsed();
            return true;
        }

        // When the left converted pylon is taken, it opens up two more pylons
        if (tile == (topLeft + BWAPI::TilePosition(6, 0)) && size.x == 2 && size.y == 2)
        {
            small.emplace_back(topLeft + BWAPI::TilePosition(4, 0), true);
            small.emplace_back(topLeft + BWAPI::TilePosition(2, 0), true);
            removeUsed();
            return true;
        }

        return true;
    }

protected:
    void placeLocations() override
    {
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 0));
        small.emplace_back(topLeft);
        medium.emplace_back(topLeft + BWAPI::TilePosition(2, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(10, 0));
        medium.emplace_back(topLeft + BWAPI::TilePosition(13, 0));
        large.emplace_back(topLeft + BWAPI::TilePosition(0, 2));
        large.emplace_back(topLeft + BWAPI::TilePosition(4, 2));
        large.emplace_back(topLeft + BWAPI::TilePosition(8, 2));
        large.emplace_back(topLeft + BWAPI::TilePosition(12, 2));
        medium.emplace_back(topLeft + BWAPI::TilePosition(1, 2), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(5, 2), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(8, 2), true);
        medium.emplace_back(topLeft + BWAPI::TilePosition(12, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 0), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(0, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(2, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(4, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(6, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(8, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(10, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(12, 2), true);
        small.emplace_back(topLeft + BWAPI::TilePosition(14, 2), true);
    }
};